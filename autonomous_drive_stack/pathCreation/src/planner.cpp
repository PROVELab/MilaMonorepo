#include "prove/planner.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace prove::planning {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double two_pi = 2.0 * pi;

double angle(double x) { return std::remainder(x, two_pi); }
bool finitePose(Pose p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.yaw); }

Pose integrate(Pose p, double curvature, double length) {
    if (std::abs(curvature) < 1e-12)
        return {p.x + length * std::cos(p.yaw), p.y + length * std::sin(p.yaw), angle(p.yaw)};
    double yaw = p.yaw + length * curvature;
    return {p.x + (std::sin(yaw) - std::sin(p.yaw)) / curvature,
            p.y - (std::cos(yaw) - std::cos(p.yaw)) / curvature, angle(yaw)};
}

// Separating-axis rectangle/square intersection. Touching an occupied cell
// counts as collision, including contact at a cell corner.
bool footprintFree(const OccupancyGrid& grid, const Config& cfg, Pose p, double extra = 0.0) {
    const double front = cfg.front_extent + cfg.safety_margin + extra;
    const double rear = cfg.rear_extent + cfg.safety_margin + extra;
    const double hw = cfg.half_width + cfg.safety_margin + extra;
    const double c = std::cos(p.yaw), s = std::sin(p.yaw);
    const double cx = p.x + (front - rear) * 0.5 * c;
    const double cy = p.y + (front - rear) * 0.5 * s;
    const double hl = (front + rear) * 0.5;
    const double ex = std::abs(c) * hl + std::abs(s) * hw;
    const double ey = std::abs(s) * hl + std::abs(c) * hw;
    const double minx = cx - ex, maxx = cx + ex, miny = cy - ey, maxy = cy + ey;
    const double endx = grid.origin_x + grid.width * grid.resolution;
    const double endy = grid.origin_y + grid.height * grid.resolution;
    if (!(minx > grid.origin_x && miny > grid.origin_y && maxx < endx && maxy < endy)) return false;
    // Include cells on both sides when the rectangle touches a grid line.
    const auto x0 = static_cast<std::size_t>(std::max(0.0, std::floor((minx - grid.origin_x) / grid.resolution) - 1.0));
    const auto y0 = static_cast<std::size_t>(std::max(0.0, std::floor((miny - grid.origin_y) / grid.resolution) - 1.0));
    const auto x1 = static_cast<std::size_t>(std::floor((maxx - grid.origin_x) / grid.resolution));
    const auto y1 = static_cast<std::size_t>(std::floor((maxy - grid.origin_y) / grid.resolution));
    const double h = grid.resolution * 0.5;
    for (std::size_t y = y0; y <= y1; ++y) {
        for (std::size_t x = x0; x <= x1; ++x) {
            const int value = grid.cells[y * grid.width + x];
            if (value >= 0 && value < cfg.occupied_threshold) continue;
            const double dx = grid.origin_x + (x + 0.5) * grid.resolution - cx;
            const double dy = grid.origin_y + (y + 0.5) * grid.resolution - cy;
            if (std::abs(dx) <= ex + h && std::abs(dy) <= ey + h &&
                std::abs(dx * c + dy * s) <= hl + h * (std::abs(c) + std::abs(s)) &&
                std::abs(-dx * s + dy * c) <= hw + h * (std::abs(c) + std::abs(s))) return false;
        }
    }
    return true;
}

struct Key {
    std::size_t x, y;
    int heading, steering;
    bool operator==(const Key& o) const { return x == o.x && y == o.y && heading == o.heading && steering == o.steering; }
};
struct Hash {
    std::size_t operator()(const Key& k) const {
        std::size_t h = std::hash<std::size_t>{}(k.x);
        for (auto v : {k.y, static_cast<std::size_t>(k.heading), static_cast<std::size_t>(k.steering)})
            h ^= std::hash<std::size_t>{}(v) + 0x9e3779b9U + (h << 6) + (h >> 2);
        return h;
    }
};
struct Node { Pose pose; double g; std::size_t parent; int steering; };
struct QueueItem {
    double f; std::size_t node;
    bool operator>(const QueueItem& o) const { return f > o.f; }
};

bool valid(const OccupancyGrid& g, Pose start, Pose goal, const Config& c) {
    const std::array<double, 14> nums = {g.resolution, g.origin_x, g.origin_y, c.wheelbase,
        c.max_steering, c.front_extent, c.rear_extent, c.half_width, c.safety_margin,
        c.primitive_length, c.goal_tolerance, c.goal_yaw_tolerance, c.steering_penalty, c.steering_change_penalty};
    for (double v : nums) if (!std::isfinite(v)) return false;
    if (!finitePose(start) || !finitePose(goal) || g.width == 0 || g.height == 0 ||
        g.width > std::numeric_limits<std::size_t>::max() / g.height ||
        g.cells.size() != g.width * g.height || g.resolution <= 0 ||
        !std::isfinite(g.origin_x + g.width * g.resolution) ||
        !std::isfinite(g.origin_y + g.height * g.resolution) ||
        c.wheelbase <= 0 || c.max_steering <= 0 || c.max_steering >= pi / 2 ||
        c.front_extent <= 0 || c.rear_extent < 0 || c.half_width <= 0 || c.safety_margin < 0 ||
        c.primitive_length <= 0 || c.goal_tolerance < 0 || c.goal_yaw_tolerance < 0 ||
        c.goal_yaw_tolerance > pi || c.steering_penalty < 0 || c.steering_change_penalty < 0 ||
        c.heading_bins < 4 || c.heading_bins > 10000 || c.max_expansions <= 0 ||
        c.occupied_threshold < 1 || c.occupied_threshold > 100) return false;
    // Bound numerical work and guard conversions to integral sample counts.
    const double radius = std::hypot(std::max(c.front_extent, c.rear_extent) + c.safety_margin,
                                     c.half_width + c.safety_margin);
    const double max_curve = std::tan(c.max_steering) / c.wheelbase;
    return std::isfinite(radius) && std::isfinite(max_curve) &&
           std::isfinite(c.primitive_length * (1 + radius * max_curve)) &&
           c.primitive_length * (1 + radius * max_curve) / (g.resolution * 0.25) <= 10000;
}
}  // namespace

Result plan(const OccupancyGrid& grid, Pose start, Pose goal, const Config& cfg) {
    Result result;
    if (!valid(grid, start, goal, cfg)) { result.reason = "invalid grid, pose, or configuration"; return result; }
    start.yaw = angle(start.yaw); goal.yaw = angle(goal.yaw);
    if (!footprintFree(grid, cfg, start)) { result.reason = "start footprint blocked or outside grid"; return result; }
    const std::array<double, 5> steers = {-cfg.max_steering, -cfg.max_steering / 2.0, 0.0,
                                           cfg.max_steering / 2.0, cfg.max_steering};
    const auto keyFor = [&](Pose p, int steer) {
        int heading = static_cast<int>(std::floor((angle(p.yaw) + pi) * cfg.heading_bins / two_pi + 0.5)) % cfg.heading_bins;
        return Key{static_cast<std::size_t>(std::floor((p.x - grid.origin_x) / grid.resolution)),
                   static_cast<std::size_t>(std::floor((p.y - grid.origin_y) / grid.resolution)), heading, steer};
    };
    const auto heuristic = [&](Pose p) { return std::max(0.0, std::hypot(p.x - goal.x, p.y - goal.y) - cfg.goal_tolerance); };
    const auto reached = [&](Pose p) { return std::hypot(p.x - goal.x, p.y - goal.y) <= cfg.goal_tolerance &&
        (!cfg.require_goal_yaw || std::abs(angle(p.yaw - goal.yaw)) <= cfg.goal_yaw_tolerance); };
    const double body_radius = std::hypot(std::max(cfg.front_extent, cfg.rear_extent) + cfg.safety_margin,
                                          cfg.half_width + cfg.safety_margin);
    const auto sampleCount = [&](double curvature) {
        return std::max(1, static_cast<int>(std::ceil(cfg.primitive_length * (1.0 + body_radius * std::abs(curvature)) /
                                                     (grid.resolution * 0.25))));
    };
    std::vector<Node> nodes;
    nodes.push_back({start, 0.0, 0, 2});
    std::unordered_map<Key, std::size_t, Hash> best;
    best[keyFor(start, 2)] = 0;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
    open.push({heuristic(start), 0});
    while (!open.empty() && result.expanded < cfg.max_expansions) {
        const auto item = open.top(); open.pop();
        const Node node = nodes[item.node];
        auto known = best.find(keyFor(node.pose, node.steering));
        if (known == best.end() || known->second != item.node) continue;
        ++result.expanded;
        if (reached(node.pose)) {
            std::vector<std::size_t> chain;
            for (std::size_t i = item.node; i != 0; i = nodes[i].parent) chain.push_back(i);
            std::reverse(chain.begin(), chain.end());
            // Optimize the steering sequence, rather than moving points off the
            // bicycle model. Every candidate is re-integrated and collision checked.
            std::vector<double> controls;
            for (auto i : chain) controls.push_back(steers[nodes[i].steering]);
            const auto original = controls;
            const auto score = [&](const std::vector<double>& u) {
                double value = 0.0, previous = 0.0;
                for (std::size_t i = 0; i < u.size(); ++i) {
                    const double change = u[i] - previous;
                    const double deviation = u[i] - original[i];
                    value += change * change + 0.05 * deviation * deviation;
                    previous = u[i];
                }
                return value;
            };
            const auto evaluate = [&](const std::vector<double>& u, std::vector<PathPoint>* output) {
                Pose p = start;
                if (output) { output->clear(); output->push_back({p, 0.0, 0.0}); }
                for (double steering : u) {
                    const double curvature = std::tan(steering) / cfg.wheelbase;
                    const int count = sampleCount(curvature);
                    const double step = cfg.primitive_length / count;
                    const double extra = step * (1.0 + body_radius * std::abs(curvature));
                    const Pose parent = p;
                    for (int j = 1; j <= count; ++j) {
                        p = integrate(parent, curvature, step * j);
                        if (!footprintFree(grid, cfg, p, extra)) return false;
                        if (output) output->push_back({p, steering, curvature});
                    }
                }
                return reached(p);
            };
            if (cfg.smooth_path && !controls.empty()) {
                double best_score = score(controls);
                for (double step : {cfg.max_steering / 4.0, cfg.max_steering / 8.0,
                                    cfg.max_steering / 16.0}) {
                    for (std::size_t i = 0; i < controls.size(); ++i) {
                        const double saved = controls[i];
                        double best_value = saved;
                        for (double delta : {-step, step}) {
                            controls[i] = std::clamp(saved + delta, -cfg.max_steering, cfg.max_steering);
                            const double candidate = score(controls);
                            if (candidate + 1e-12 < best_score && evaluate(controls, nullptr)) {
                                best_score = candidate;
                                best_value = controls[i];
                            }
                        }
                        controls[i] = best_value;
                    }
                }
                if (controls != original && evaluate(controls, &result.path)) {
                    result.smoothing_applied = true;
                    result.cost = node.g; result.success = true; return result;
                }
            }
            result.path.push_back({start, 0.0, 0.0});
            for (std::size_t i : chain) {
                const Node& child = nodes[i];
                const Pose parent = nodes[child.parent].pose;
                const double steer = steers[child.steering], curve = std::tan(steer) / cfg.wheelbase;
                const int count = sampleCount(curve);
                for (int j = 1; j <= count; ++j)
                    result.path.push_back({integrate(parent, curve, cfg.primitive_length * j / count), steer, curve});
            }
            result.cost = node.g; result.success = true; return result;
        }
        for (int si = 0; si < static_cast<int>(steers.size()); ++si) {
            const double steer = steers[si], curve = std::tan(steer) / cfg.wheelbase;
            const int count = sampleCount(curve);
            const double step = cfg.primitive_length / count;
            // Each footprint is enlarged by a bound on the movement of every
            // body point over one sample interval, covering the swept area.
            const double extra = step * (1.0 + body_radius * std::abs(curve));
            bool free = true;
            for (int j = 1; j <= count; ++j) {
                if (!footprintFree(grid, cfg, integrate(node.pose, curve, step * j), extra)) { free = false; break; }
            }
            if (!free) continue;
            Pose next = integrate(node.pose, curve, cfg.primitive_length);
            const Key key = keyFor(next, si);
            const double cost = node.g + cfg.primitive_length + cfg.steering_penalty * std::abs(steer) * cfg.primitive_length +
                                cfg.steering_change_penalty * std::abs(steer - steers[node.steering]);
            auto it = best.find(key);
            if (it != best.end() && cost >= nodes[it->second].g) continue;
            const std::size_t id = nodes.size();
            nodes.push_back({next, cost, item.node, si});
            best[key] = id;
            open.push({cost + heuristic(next), id});
        }
    }
    result.reason = result.expanded >= cfg.max_expansions ? "search expansion limit reached" : "no collision-free path";
    return result;
}
}  // namespace prove::planning
