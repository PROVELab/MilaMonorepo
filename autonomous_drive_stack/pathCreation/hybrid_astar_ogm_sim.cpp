// Hybrid A* + Occupancy Grid Map (OGM) simulator for macOS + raylib.
//
// This replaces a Delaunay-triangle state with a car-like state:
// (occupancy-grid cell, heading bin). Each transition is a bicycle-model arc,
// so every returned path respects the configured steering limit / turn radius.
//
// Build:
// clang++ -std=c++17 -O2 hybrid_astar_ogm_sim.cpp -o hybrid_astar_ogm_sim $(pkg-config --cflags --libs raylib)
// ./hybrid_astar_ogm_sim

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// raylib defines a PI macro, so use project-specific names here.
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

struct Point { double x, y; };
struct Pose { double x, y, yaw; };
struct Cell { int col, row; };

double distance(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
double normalizeAngle(double a) {
    while (a >= kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

// Vehicle coordinate convention: +x forward, +y left, ego vehicle at (0, 0).
Point worldToCar(Point p, Pose car) {
    const double dx = p.x - car.x, dy = p.y - car.y;
    const double c = std::cos(car.yaw), s = std::sin(car.yaw);
    return {c * dx + s * dy, -s * dx + c * dy};
}
Point carToWorld(Point p, Pose car) {
    const double c = std::cos(car.yaw), s = std::sin(car.yaw);
    return {car.x + c * p.x - s * p.y, car.y + s * p.x + c * p.y};
}

class OccupancyGrid {
public:
    OccupancyGrid(double min_x, double max_x, double min_y, double max_y, double resolution)
        : min_x_(min_x), max_x_(max_x), min_y_(min_y), max_y_(max_y), res_(resolution),
          width_(static_cast<int>(std::ceil((max_x - min_x) / resolution))),
          height_(static_cast<int>(std::ceil((max_y - min_y) / resolution))),
          blocked_(width_ * height_, false) {}

    Cell toCell(Point p) const {
        if (p.x < min_x_ || p.x >= max_x_ || p.y < min_y_ || p.y >= max_y_) return {-1, -1};
        return {static_cast<int>((p.x - min_x_) / res_), static_cast<int>((p.y - min_y_) / res_)};
    }
    Point center(Cell c) const { return {min_x_ + (c.col + .5) * res_, min_y_ + (c.row + .5) * res_}; }
    bool isFree(Point p) const {
        const Cell c = toCell(p);
        return c.col >= 0 && !blocked_[index(c)];
    }
    void block(Cell c) {
        if (c.col >= 0 && c.col < width_ && c.row >= 0 && c.row < height_) blocked_[index(c)] = true;
    }
    int width() const { return width_; }
    int height() const { return height_; }
    double resolution() const { return res_; }
    int index(Cell c) const { return c.row * width_ + c.col; }
    Cell fromIndex(int i) const { return {i % width_, i / width_}; }

private:
    double min_x_, max_x_, min_y_, max_y_, res_;
    int width_, height_;
    std::vector<bool> blocked_;
};

double pointToSegmentDistance(Point p, Point a, Point b) {
    const double dx = b.x - a.x, dy = b.y - a.y, length_sq = dx * dx + dy * dy;
    if (length_sq == 0.0) return distance(p, a);
    const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length_sq, 0.0, 1.0);
    return distance(p, {a.x + t * dx, a.y + t * dy});
}

double distanceToPolyline(Point p, const std::vector<Point>& line) {
    double closest = std::numeric_limits<double>::infinity();
    for (size_t i = 1; i < line.size(); ++i) closest = std::min(closest, pointToSegmentDistance(p, line[i - 1], line[i]));
    return closest;
}

bool insidePolygon(Point p, const std::vector<Point>& polygon) {
    bool inside = false;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const Point a = polygon[i], b = polygon[(i + 1) % polygon.size()];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double x_at_y = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;
            if (p.x < x_at_y) inside = !inside;
        }
    }
    return inside;
}

// A lane cell is valid only if it lies inside the edge polygon AND has enough
// clearance for the car body. This turns perception's edge points into a
// genuinely drivable (not just visually bounded) corridor.
OccupancyGrid buildOGM(const std::vector<Point>& left_edge,
                       const std::vector<Point>& right_edge,
                       const std::vector<Point>& obstacle_centers,
                       double vehicle_half_width,
                       double safety_margin,
                       double obstacle_radius,
                       bool use_track_boundaries = true) {
    constexpr double MIN_X = -2.0, MAX_X = 35.0, MIN_Y = -12.0, MAX_Y = 12.0, RES = .25;
    OccupancyGrid grid(MIN_X, MAX_X, MIN_Y, MAX_Y, RES);
    const double boundary_clearance = vehicle_half_width + safety_margin;
    if (use_track_boundaries) {
        if (left_edge.size() < 2 || right_edge.size() < 2) throw std::runtime_error("Need ordered left and right lane edges");
        std::vector<Point> corridor = left_edge;
        corridor.insert(corridor.end(), right_edge.rbegin(), right_edge.rend());

        // Quick fail-safe: if perception reports a corridor narrower than the car
        // plus clearance, do not invent a route through it.
        const size_t samples = std::min(left_edge.size(), right_edge.size());
        for (size_t i = 0; i < samples; ++i) {
            const size_t li = i * (left_edge.size() - 1) / std::max<size_t>(1, samples - 1);
            const size_t ri = i * (right_edge.size() - 1) / std::max<size_t>(1, samples - 1);
            if (distance(left_edge[li], right_edge[ri]) <= 2.0 * boundary_clearance + .15) {
                throw std::runtime_error("lane is narrower than car width plus safety margins");
            }
        }

        for (int row = 0; row < grid.height(); ++row) {
            for (int col = 0; col < grid.width(); ++col) {
                const Cell cell{col, row};
                const Point p = grid.center(cell);
                if (!insidePolygon(p, corridor) ||
                    distanceToPolyline(p, left_edge) < boundary_clearance ||
                    distanceToPolyline(p, right_edge) < boundary_clearance) {
                    grid.block(cell);
                }
            }
        }

    }

    // Point obstacles from perception become discs enlarged by the car radius.
    const double inflated_obstacle = obstacle_radius + boundary_clearance;
    for (Point obstacle : obstacle_centers) {
        for (int row = 0; row < grid.height(); ++row)
            for (int col = 0; col < grid.width(); ++col)
                if (distance(grid.center({col, row}), obstacle) <= inflated_obstacle) grid.block({col, row});
    }
    return grid;
}

std::vector<double> cumulativeLengths(const std::vector<Point>& line) {
    std::vector<double> lengths{0.0};
    for (size_t i = 1; i < line.size(); ++i) lengths.push_back(lengths.back() + distance(line[i - 1], line[i]));
    return lengths;
}

Point pointAtDistance(const std::vector<Point>& line, double target) {
    const auto lengths = cumulativeLengths(line);
    if (line.size() < 2 || target > lengths.back()) throw std::runtime_error("Track edges do not reach the 30 m goal");
    for (size_t i = 0; i + 1 < line.size(); ++i) {
        if (lengths[i] <= target && target <= lengths[i + 1]) {
            const double t = (target - lengths[i]) / (lengths[i + 1] - lengths[i]);
            return {line[i].x + t * (line[i + 1].x - line[i].x), line[i].y + t * (line[i + 1].y - line[i].y)};
        }
    }
    return line.back();
}

// Arc-length position of the nearest point on a polyline. This lets a goal be
// "30 m ahead of the vehicle", even though the observed edge polylines retain
// a few metres of points behind the car.
double closestArcLength(const std::vector<Point>& line, Point query) {
    const auto lengths = cumulativeLengths(line);
    double best_distance_sq = std::numeric_limits<double>::infinity();
    double best_arc_length = 0.0;
    for (size_t i = 1; i < line.size(); ++i) {
        const double dx = line[i].x - line[i - 1].x, dy = line[i].y - line[i - 1].y;
        const double length_sq = dx * dx + dy * dy;
        const double t = std::clamp(((query.x - line[i - 1].x) * dx + (query.y - line[i - 1].y) * dy) / length_sq, 0.0, 1.0);
        const Point projected{line[i - 1].x + t * dx, line[i - 1].y + t * dy};
        const double d_sq = (projected.x - query.x) * (projected.x - query.x) + (projected.y - query.y) * (projected.y - query.y);
        if (d_sq < best_distance_sq) {
            best_distance_sq = d_sq;
            best_arc_length = lengths[i - 1] + t * std::sqrt(length_sq);
        }
    }
    return best_arc_length;
}

std::vector<Point> makeCenterline(const std::vector<Point>& left, const std::vector<Point>& right) {
    const double usable = std::min(cumulativeLengths(left).back(), cumulativeLengths(right).back());
    std::vector<Point> center;
    for (double s = 0.0; s <= usable; s += .5) {
        const Point a = pointAtDistance(left, s), b = pointAtDistance(right, s);
        center.push_back({(a.x + b.x) / 2.0, (a.y + b.y) / 2.0});
    }
    return center;
}

struct HybridConfig {
    double wheelbase_m = 1.6;
    double max_steering_rad = .42;  // minimum turn radius = wheelbase / tan(max steering)
    double primitive_length_m = .75;
    int heading_bins = 24;
    double goal_tolerance_m = .8;
    int max_expansions = 50000;
};

struct HybridState {
    int col, row, heading;
    bool operator==(const HybridState& other) const {
        return col == other.col && row == other.row && heading == other.heading;
    }
};

struct HybridStateHash {
    size_t operator()(const HybridState& s) const {
        const size_t a = static_cast<size_t>(s.col) * 73856093U;
        const size_t b = static_cast<size_t>(s.row) * 19349663U;
        const size_t c = static_cast<size_t>(s.heading) * 83492791U;
        return a ^ b ^ c;
    }
};

struct HybridResult {
    bool found = false;
    double total_cost = std::numeric_limits<double>::infinity();
    int expanded = 0;
    std::string reason;
    std::vector<Pose> poses;
};

// Play back the planner's constant-curvature arcs at a speed in metres/second.
// Keep the path in world coordinates so replanning can use the moving car frame.
class PathFollower {
public:
    void setPath(const std::vector<Pose>& local_path, Pose car) {
        path_.clear();
        for (const Pose& pose : local_path) {
            const Point world = carToWorld({pose.x, pose.y}, car);
            path_.push_back({world.x, world.y, normalizeAngle(car.yaw + pose.yaw)});
        }
        next_ = 1;
        progress_ = 0.0;
    }

    void advance(Pose& car, double travel) {
        while (travel > 0.0 && next_ < path_.size()) {
            const Pose a = path_[next_ - 1], b = path_[next_];
            const double angle = normalizeAngle(b.yaw - a.yaw);
            const double chord = distance({a.x, a.y}, {b.x, b.y});
            const double length = std::abs(angle) < 1e-9
                ? chord : chord * angle / (2.0 * std::sin(angle / 2.0));
            const double step = std::min(travel, std::max(0.0, length - progress_));
            progress_ += step;
            travel -= step;
            if (progress_ >= length) {
                car = b;
                ++next_;
                progress_ = 0.0;
                continue;
            }
            car.yaw = normalizeAngle(a.yaw + angle * progress_ / length);
            if (std::abs(angle) < 1e-9) {
                const double t = progress_ / length;
                car.x = a.x + t * (b.x - a.x);
                car.y = a.y + t * (b.y - a.y);
            } else {
                const double curvature = angle / length;
                car.x = a.x + (std::sin(car.yaw) - std::sin(a.yaw)) / curvature;
                car.y = a.y - (std::cos(car.yaw) - std::cos(a.yaw)) / curvature;
            }
        }
    }

private:
    std::vector<Pose> path_;
    size_t next_ = 1;
    double progress_ = 0.0;
};

class HybridAStar {
public:
    explicit HybridAStar(HybridConfig config = {}) : cfg_(config) {}

    HybridResult findPath(const OccupancyGrid& grid, Pose start, Point goal) const {
        if (!grid.isFree({start.x, start.y})) return {false, {}, 0, "start is blocked", {}};
        if (!grid.isFree(goal)) return {false, {}, 0, "goal is blocked", {}};
        const HybridState start_state = makeState(grid, start);
        if (start_state.col < 0) return {false, {}, 0, "start outside grid", {}};

        struct OpenItem {
            HybridState state; double g; double f;
            bool operator>(const OpenItem& rhs) const { return f > rhs.f; }
        };
        std::priority_queue<OpenItem, std::vector<OpenItem>, std::greater<OpenItem>> open;
        std::unordered_map<HybridState, NodeRecord, HybridStateHash> records;
        records.emplace(start_state, NodeRecord{0.0, start, {}, false});
        open.push({start_state, 0.0, distance({start.x, start.y}, goal)});

        const std::array<double, 3> steering = {-cfg_.max_steering_rad, 0.0, cfg_.max_steering_rad};
        int expanded = 0;
        while (!open.empty() && expanded < cfg_.max_expansions) {
            const OpenItem current_open = open.top(); open.pop();
            const auto current_it = records.find(current_open.state);
            if (current_it == records.end() || current_open.g != current_it->second.g) continue;
            const NodeRecord current = current_it->second;
            ++expanded;

            if (distance({current.pose.x, current.pose.y}, goal) <= cfg_.goal_tolerance_m) {
                return reconstruct(current_open.state, records, current.g, expanded);
            }

            for (const double steer : steering) {
                if (!arcIsFree(grid, current.pose, steer)) continue;
                const Pose next_pose = integrate(current.pose, steer, cfg_.primitive_length_m);
                const HybridState next = makeState(grid, next_pose);
                if (next.col < 0) continue;
                // Travel distance is the primary cost. Small steering cost avoids
                // needless left-right wiggles, while Euclidean heuristic remains safe.
                const double candidate = current.g + cfg_.primitive_length_m + .04 * std::abs(steer);
                const auto known = records.find(next);
                if (known != records.end() && candidate >= known->second.g) continue;
                records[next] = {candidate, next_pose, current_open.state, true};
                open.push({next, candidate, candidate + distance({next_pose.x, next_pose.y}, goal)});
            }
        }
        const std::string reason = expanded >= cfg_.max_expansions ? "search expansion limit reached" : "no collision-free kinematic path";
        return {false, std::numeric_limits<double>::infinity(), expanded, reason, {}};
    }

    double minTurnRadius() const { return cfg_.wheelbase_m / std::tan(cfg_.max_steering_rad); }
    const HybridConfig& config() const { return cfg_; }

private:
    struct NodeRecord { double g; Pose pose; HybridState parent; bool has_parent; };
    HybridConfig cfg_;

    HybridState makeState(const OccupancyGrid& grid, Pose pose) const {
        const Cell cell = grid.toCell({pose.x, pose.y});
        if (cell.col < 0) return {-1, -1, -1};
        const double unit = kTwoPi / cfg_.heading_bins;
        int heading = static_cast<int>(std::floor((normalizeAngle(pose.yaw) + kPi) / unit + .5)) % cfg_.heading_bins;
        if (heading < 0) heading += cfg_.heading_bins;
        return {cell.col, cell.row, heading};
    }

    Pose integrate(Pose start, double steering, double travel) const {
        const double curvature = std::tan(steering) / cfg_.wheelbase_m;
        if (std::abs(curvature) < 1e-9) {
            return {start.x + travel * std::cos(start.yaw), start.y + travel * std::sin(start.yaw), start.yaw};
        }
        const double next_yaw = normalizeAngle(start.yaw + curvature * travel);
        return {start.x + (std::sin(next_yaw) - std::sin(start.yaw)) / curvature,
                start.y - (std::cos(next_yaw) - std::cos(start.yaw)) / curvature,
                next_yaw};
    }

    bool arcIsFree(const OccupancyGrid& grid, Pose start, double steering) const {
        const int samples = std::max(2, static_cast<int>(std::ceil(cfg_.primitive_length_m / (grid.resolution() * .45))));
        for (int i = 1; i <= samples; ++i) {
            const Pose sample = integrate(start, steering, cfg_.primitive_length_m * i / samples);
            if (!grid.isFree({sample.x, sample.y})) return false;
        }
        return true;
    }

    HybridResult reconstruct(
        HybridState goal,
        const std::unordered_map<HybridState, NodeRecord, HybridStateHash>&,
        double, int) const;
};

HybridResult HybridAStar::reconstruct(
    HybridState goal,
    const std::unordered_map<HybridState, HybridAStar::NodeRecord, HybridStateHash>& records,
    double cost,
    int expanded) const {
    std::vector<Pose> reversed;
    HybridState current = goal;
    while (true) {
        const auto it = records.find(current);
        if (it == records.end()) return {false, {}, expanded, "broken predecessor chain", {}};
        reversed.push_back(it->second.pose);
        if (!it->second.has_parent) break;
        current = it->second.parent;
    }
    std::reverse(reversed.begin(), reversed.end());
    return {true, cost, expanded, "", reversed};
}

// Static test course. In your vehicle, replace this with perception outputs:
// ordered left edge, ordered right edge, and static-obstacle positions.
struct Track { std::vector<Point> left, right, obstacles; };

// Obstacle positions are fixed world coordinates in meters.
Track makeTrack(const std::vector<Point>& obstacles) {
    Track track;
    constexpr double lane_half_width = 4.8;  // 9.6 m wide: feasible after clearance erosion.
    for (double x = -5.0; x <= 160.0; x += .35) {
        const double y = 2.7 * std::sin(x / 29.0);
        const double dydx = (2.7 / 29.0) * std::cos(x / 29.0);
        const double norm = std::hypot(dydx, 1.0);
        const Point normal{-dydx / norm, 1.0 / norm};
        track.left.push_back({x + lane_half_width * normal.x, y + lane_half_width * normal.y});
        track.right.push_back({x - lane_half_width * normal.x, y - lane_half_width * normal.y});
    }
    track.obstacles = obstacles;  // Inflated for collision checking in buildOGM.
    return track;
}

// Append custom world-coordinate obstacles to an existing track.
void addObstacles(Track& track, const std::vector<Point>& obstacles) {
    track.obstacles.insert(track.obstacles.end(), obstacles.begin(), obstacles.end());
}

// Accept either "x, y" or "x y", rejecting extra or non-finite values.
bool parseObstacleCoordinates(std::string text, Point& point) {
    const auto comma = text.find(',');
    if (comma != std::string::npos) text[comma] = ' ';
    std::istringstream input(text);
    Point parsed{};
    if (!(input >> parsed.x >> parsed.y)) return false;
    input >> std::ws;
    if (!input.eof() || !std::isfinite(parsed.x) || !std::isfinite(parsed.y)) return false;
    point = parsed;
    return true;
}

std::vector<Point> visibleInCarFrame(const std::vector<Point>& world, Pose car) {
    std::vector<Point> result;
    for (Point point : world) {
        point = worldToCar(point, car);
        // Keep extra forward edge points. Do NOT throw away an edge merely
        // because it is laterally far while the car is turning; doing that can
        // make a valid 30 m corridor look artificially too short.
        if (point.x >= -5.0 && point.x <= 45.0) result.push_back(point);
    }
    return result;
}

// The demo track gives corresponding left/right samples. Filter those pairs
// together so the centerline remains geometrically meaningful as the car turns.
// With real perception, first resample both ordered edge polylines by arc length
// and then use the same idea.
void visibleEdgePairsInCarFrame(const std::vector<Point>& left_world,
                                const std::vector<Point>& right_world,
                                Pose car,
                                std::vector<Point>& left_out,
                                std::vector<Point>& right_out) {
    left_out.clear(); right_out.clear();
    const size_t count = std::min(left_world.size(), right_world.size());
    for (size_t i = 0; i < count; ++i) {
        const Point left = worldToCar(left_world[i], car);
        const Point right = worldToCar(right_world[i], car);
        const double midpoint_x = (left.x + right.x) / 2.0;
        if (midpoint_x >= -5.0 && midpoint_x <= 45.0) {
            left_out.push_back(left);
            right_out.push_back(right);
        }
    }
}

Vector2 toScreen(Point point, Pose car) {
    constexpr float SCALE = 16.0f;
    return {235.0f + static_cast<float>((point.x - car.x) * SCALE),
            410.0f - static_cast<float>((point.y - car.y) * SCALE)};
}

// The inverse of toScreen(). This is simulator-only code: it converts a mouse
// click into a world-coordinate goal or obstacle for the planner.
Point screenToWorld(Vector2 screen, Pose car) {
    constexpr double SCALE = 16.0;
    return {car.x + (screen.x - 235.0) / SCALE,
            car.y - (screen.y - 410.0) / SCALE};
}

void drawPolyline(const std::vector<Point>& line, Pose car, Color color, float thickness) {
    for (size_t i = 1; i < line.size(); ++i) DrawLineEx(toScreen(line[i - 1], car), toScreen(line[i], car), thickness, color);
}

int main() {
    constexpr int WINDOW_W = 1200, WINDOW_H = 800;
    constexpr double CAR_HALF_WIDTH = .75;  // tune to the real vehicle
    constexpr double SAFETY_MARGIN = .35;
    constexpr double OBSTACLE_RADIUS = .25;
    constexpr double LOOKAHEAD_M = 30.0;

    HybridConfig config;
    HybridAStar planner(config);
    // Edit this vector to set the initial world-coordinate obstacles.
    const std::vector<Point> initial_obstacles = {
        {24.0, 2.7 * std::sin(24.0 / 29.0)},
        {65.0, 2.7 * std::sin(65.0 / 29.0) - .65}
    };
    Track track = makeTrack(initial_obstacles);
    // Start aligned with the tangent of the test course.
    Pose car{0.0, 0.0, std::atan(2.7 / 29.0)};
    HybridResult plan;
    PathFollower follower;
    bool paused = false;
    bool use_track_boundaries = false;
    std::vector<Point> world_path;
    std::string planner_status = "waiting to plan";
    double replan_timer = 1.0;
    bool has_clicked_goal = false;
    Point clicked_goal_world{};
    Point displayed_goal_world{};
    bool entering_obstacle = false;
    std::string obstacle_input;
    std::string input_error;
    int obstacle_scroll = 0;
    constexpr int PANEL_X = 870;
    constexpr int VISIBLE_OBSTACLES = 22;

    InitWindow(WINDOW_W, WINDOW_H, "Hybrid A* OGM Vehicle Simulator");
    SetExitKey(KEY_NULL);  // Escape cancels coordinate entry before it can exit.
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        const double dt = std::min(.04f, GetFrameTime());
        const bool input_was_active = entering_obstacle;
        if (!entering_obstacle && IsKeyPressed(KEY_ESCAPE)) break;
        if (!entering_obstacle && IsKeyPressed(KEY_O)) {
            entering_obstacle = true;
            obstacle_input.clear();
            input_error.clear();
            while (GetCharPressed() != 0) {}  // Discard the opening shortcut.
        } else if (entering_obstacle) {
            for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed()) {
                if (ch >= 32 && ch <= 126 && obstacle_input.size() < 32) {
                    obstacle_input += static_cast<char>(ch);
                    input_error.clear();
                }
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !obstacle_input.empty()) obstacle_input.pop_back();
            if (IsKeyPressed(KEY_ESCAPE)) entering_obstacle = false;
            if (IsKeyPressed(KEY_ENTER)) {
                Point obstacle{};
                if (parseObstacleCoordinates(obstacle_input, obstacle)) {
                    addObstacles(track, {obstacle});
                    replan_timer = .08;
                    obstacle_scroll = std::max(0, static_cast<int>(track.obstacles.size()) - VISIBLE_OBSTACLES);
                    entering_obstacle = false;
                } else {
                    input_error = "Enter two finite numbers: x, y";
                }
            }
        }
        // Keep normal shortcuts and scene clicks out of the coordinate editor.
        if (!input_was_active && !entering_obstacle) {
        if (IsKeyPressed(KEY_R)) {
            car = {0.0, 0.0, std::atan(2.7 / 29.0)};
            replan_timer = 1.0;
        }
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_B)) {
            use_track_boundaries = !use_track_boundaries;
            replan_timer = .08;
        }
        // SIMULATOR ONLY: left-click a destination. A returns to the
        // default goal, 30 m ahead along the lane centerline.
        const bool mouse_in_scene = GetMousePosition().y >= 100.0f && GetMousePosition().x < PANEL_X;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_in_scene) {
            clicked_goal_world = screenToWorld(GetMousePosition(), car);
            has_clicked_goal = true;
            replan_timer = .08;  // Replan immediately for the new destination.
        }
        if (IsKeyPressed(KEY_A)) {
            has_clicked_goal = false;
            replan_timer = .08;
        }
        // Right-click the scene to place a fixed obstacle and replan now.
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && mouse_in_scene) {
            addObstacles(track, {screenToWorld(GetMousePosition(), car)});
            replan_timer = .08;
        }
        }
        if (GetMousePosition().x >= PANEL_X) {
            obstacle_scroll -= static_cast<int>(GetMouseWheelMove() * 3);
        }
        obstacle_scroll = std::clamp(obstacle_scroll, 0,
            std::max(0, static_cast<int>(track.obstacles.size()) - VISIBLE_OBSTACLES));

        // Perception -> vehicle frame -> OGM -> heading-aware A* (12.5 Hz).
        replan_timer += dt;
        if (replan_timer >= .08) {
            replan_timer = 0.0;
            try {
                std::vector<Point> left, right;
                visibleEdgePairsInCarFrame(track.left, track.right, car, left, right);
                const auto obstacles = visibleInCarFrame(track.obstacles, car);
                const auto centerline = makeCenterline(left, right);
                const OccupancyGrid grid = buildOGM(left, right, obstacles, CAR_HALF_WIDTH, SAFETY_MARGIN, OBSTACLE_RADIUS, use_track_boundaries);
                // Shorten the horizon at the end of the course so the car can
                // finish instead of stopping when fewer than 30 metres remain.
                const double current_arc = closestArcLength(centerline, {0.0, 0.0});
                double goal_arc = std::min(
                    cumulativeLengths(centerline).back(),
                    current_arc + LOOKAHEAD_M);
                Point automatic_goal = pointAtDistance(centerline, goal_arc);
                // During a turn, 30 m ahead can lie outside the local grid.
                // Keep automatic goals in free space; clicked goals stay exact.
                while (!has_clicked_goal && !grid.isFree(automatic_goal) && goal_arc > current_arc) {
                    goal_arc = std::max(current_arc, goal_arc - grid.resolution());
                    automatic_goal = pointAtDistance(centerline, goal_arc);
                }
                // The planner always needs its goal in the CAR frame.
                const Point goal = has_clicked_goal
                    ? worldToCar(clicked_goal_world, car)
                    : automatic_goal;
                displayed_goal_world = has_clicked_goal
                    ? clicked_goal_world
                    : carToWorld(automatic_goal, car);
                plan = planner.findPath(grid, {0.0, 0.0, 0.0}, goal);
                planner_status = plan.found
                    ? (has_clicked_goal ? "PLANNING: CLICKED GOAL OK" : "PLANNING: 30 M GOAL OK")
                    : "PLANNING: " + plan.reason;
                world_path.clear();
                if (plan.found) {
                    for (const Pose& pose : plan.poses) world_path.push_back(carToWorld({pose.x, pose.y}, car));
                }
                follower.setPath(plan.poses, car);
            } catch (const std::exception& error) {
                plan = {};
                world_path.clear();
                follower.setPath({}, car);
                planner_status = std::string("PLANNING: ") + error.what();
            }
        }

        // Stop on a failed plan or at its endpoint. Each successful replan
        // replaces the remaining route with arcs starting at the current pose.
        constexpr double FOLLOW_SPEED_MPS = 4.0;
        if (!paused && !entering_obstacle && !input_was_active && plan.found) follower.advance(car, FOLLOW_SPEED_MPS * dt);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        if (use_track_boundaries) {
            drawPolyline(track.left, car, DARKGREEN, 3.0f);
            drawPolyline(track.right, car, DARKGREEN, 3.0f);
        }
        drawPolyline(world_path, car, BLUE, 3.5f);
        for (size_t i = 0; i < track.obstacles.size(); ++i) {
            const Vector2 screen = toScreen(track.obstacles[i], car);
            DrawCircleV(screen, 7.0f, RED);
            DrawText(TextFormat("#%d", static_cast<int>(i + 1)), static_cast<int>(screen.x) + 10,
                     static_cast<int>(screen.y) - 8, 16, MAROON);
        }
        const Vector2 goal_screen = toScreen(displayed_goal_world, car);
        DrawCircleLinesV(goal_screen, 11.0f, PURPLE);
        DrawLineV({goal_screen.x - 15.0f, goal_screen.y}, {goal_screen.x + 15.0f, goal_screen.y}, PURPLE);
        DrawLineV({goal_screen.x, goal_screen.y - 15.0f}, {goal_screen.x, goal_screen.y + 15.0f}, PURPLE);

        const Vector2 nose = toScreen({car.x + 1.05 * std::cos(car.yaw), car.y + 1.05 * std::sin(car.yaw)}, car);
        const Vector2 left_corner = toScreen({car.x + .60 * std::cos(car.yaw + 2.45), car.y + .60 * std::sin(car.yaw + 2.45)}, car);
        const Vector2 right_corner = toScreen({car.x + .60 * std::cos(car.yaw - 2.45), car.y + .60 * std::sin(car.yaw - 2.45)}, car);
        DrawTriangle(nose, left_corner, right_corner, BLACK);
        DrawRectangle(0, 0, WINDOW_W, 100, Fade(RAYWHITE, .94f));
        DrawText("Green: feasible lane edges  Red: obstacles  Blue: Hybrid A* path  Purple: goal  Black: vehicle", 18, 12, 20, DARKGRAY);
        DrawText(TextFormat("B: boundaries %s", use_track_boundaries ? "ON" : "OFF"), 990, 14, 16, DARKGRAY);
        DrawText(planner_status.c_str(), 18, 42, 20, plan.found ? DARKGREEN : MAROON);
        DrawText(TextFormat("%s   radius %.2f m   expanded %d   Space: pause   left-click: goal   A: auto   R: reset", paused ? "PAUSED" : "AUTO", planner.minTurnRadius(), plan.expanded), 430, 42, 15, DARKGRAY);
        DrawText("Right-click: add obstacle   O: enter obstacle coordinates   Space: pause", 18, 74, 16, DARKGRAY);
        DrawRectangle(PANEL_X, 100, WINDOW_W - PANEL_X, WINDOW_H - 100, Fade(RAYWHITE, .97f));
        DrawText("World coordinates (meters)", PANEL_X + 12, 112, 20, DARKGRAY);
        DrawText(TextFormat("Car:  x %.2f   y %.2f", car.x, car.y), PANEL_X + 12, 144, 18, BLACK);
        DrawText(TextFormat("Heading: %.1f deg", car.yaw * 180.0 / kPi), PANEL_X + 12, 170, 18, BLACK);
        DrawText(TextFormat("Goal: x %.2f   y %.2f", displayed_goal_world.x, displayed_goal_world.y), PANEL_X + 12, 196, 18, PURPLE);
        DrawText(TextFormat("Obstacles: %d (scroll here)", static_cast<int>(track.obstacles.size())), PANEL_X + 12, 230, 18, MAROON);
        for (int row = 0; row < VISIBLE_OBSTACLES && obstacle_scroll + row < static_cast<int>(track.obstacles.size()); ++row) {
            const int index = obstacle_scroll + row;
            const Point& obstacle = track.obstacles[index];
            DrawText(TextFormat("#%d: x %.2f  y %.2f", index + 1, obstacle.x, obstacle.y),
                     PANEL_X + 12, 258 + row * 24, 17, MAROON);
        }
        if (entering_obstacle) {
            DrawRectangle(100, 280, 650, 166, RAYWHITE);
            DrawRectangleLines(100, 280, 650, 166, DARKGRAY);
            DrawText("Add obstacle: world x, y in meters", 118, 296, 22, DARKGRAY);
            DrawText((obstacle_input + "_").c_str(), 118, 334, 24, BLACK);
            DrawText("Enter: add   Esc: cancel   Movement paused", 118, 376, 18, DARKGRAY);
            DrawText(input_error.c_str(), 118, 410, 18, MAROON);
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
