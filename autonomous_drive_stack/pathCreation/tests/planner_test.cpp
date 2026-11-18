#include "prove/planner.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace prove::planning;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

OccupancyGrid openGrid() {
    OccupancyGrid grid;
    grid.width = 70;
    grid.height = 40;
    grid.resolution = 0.5;
    grid.origin_x = -5.0;
    grid.origin_y = -10.0;
    grid.cells.assign(grid.width * grid.height, 0);
    return grid;
}

void fill(OccupancyGrid& grid, double min_x, double max_x, double min_y,
          double max_y, std::int8_t value) {
    for (std::size_t row = 0; row < grid.height; ++row) {
        for (std::size_t col = 0; col < grid.width; ++col) {
            const double x = grid.origin_x + (col + 0.5) * grid.resolution;
            const double y = grid.origin_y + (row + 0.5) * grid.resolution;
            if (x >= min_x && x < max_x && y >= min_y && y < max_y) {
                grid.cells[row * grid.width + col] = value;
            }
        }
    }
}

void validPath(const Result& result, Pose start, Pose goal, const Config& config) {
    check(result.success, "expected a path: " + result.reason);
    check(!result.path.empty(), "successful path is empty");
    const auto& first = result.path.front().pose;
    const auto& last = result.path.back().pose;
    check(std::hypot(first.x - start.x, first.y - start.y) < 1e-6,
          "path does not begin at the supplied pose");
    check(std::hypot(last.x - goal.x, last.y - goal.y) <= config.goal_tolerance + 1e-6,
          "path does not reach the configured goal tolerance");
    for (const auto& point : result.path) {
        check(std::isfinite(point.pose.x) && std::isfinite(point.pose.y) &&
              std::isfinite(point.pose.yaw) && std::isfinite(point.steering) &&
              std::isfinite(point.curvature), "path contains a non-finite value");
        check(std::abs(point.steering) <= config.max_steering + 1e-6,
              "path exceeds the steering limit");
        check(std::abs(point.curvature) <= std::tan(config.max_steering) / config.wheelbase + 1e-6,
              "path exceeds the curvature limit");
        check(std::abs(point.curvature - std::tan(point.steering) / config.wheelbase) < 1e-6,
              "steering and curvature disagree");
    }
    for (std::size_t i = 1; i < result.path.size(); ++i) {
        const auto& previous = result.path[i - 1].pose;
        const auto& point = result.path[i];
        const double dx = point.pose.x - previous.x;
        const double dy = point.pose.y - previous.y;
        const double yaw_change = std::remainder(point.pose.yaw - previous.yaw,
                                                 2.0 * std::acos(-1.0));
        check(std::hypot(dx, dy) <= config.primitive_length + 1e-6,
              "path has a discontinuous jump");
        if (std::abs(point.curvature) > 1e-8) {
            const double expected_x = (std::sin(point.pose.yaw) - std::sin(previous.yaw)) / point.curvature;
            const double expected_y = (std::cos(previous.yaw) - std::cos(point.pose.yaw)) / point.curvature;
            check(std::hypot(dx - expected_x, dy - expected_y) < 1e-5 &&
                  yaw_change / point.curvature >= -1e-8,
                  "samples do not follow their reported forward bicycle arc");
        } else {
            check(std::abs(yaw_change) < 1e-6 &&
                  std::abs(-std::sin(previous.yaw) * dx + std::cos(previous.yaw) * dy) < 1e-6 &&
                  std::cos(previous.yaw) * dx + std::sin(previous.yaw) * dy >= -1e-8,
                  "straight samples do not follow the heading forward");
        }
    }
}

void expectFailure(const Result& result, const std::string& message) {
    check(!result.success, message + " should fail");
    check(result.path.empty(), message + " must not return a usable stale path");
    check(!result.reason.empty(), message + " should explain the failure");
}

// Separating-axis test for a vehicle rectangle and an axis-aligned cell. This
// independently checks the body, not merely the rear-axle points.
bool overlapsCell(Pose pose, const Config& config, double x, double y, double half_cell) {
    const double c = std::cos(pose.yaw), s = std::sin(pose.yaw);
    const double half_length = (config.front_extent + config.rear_extent) / 2 + config.safety_margin;
    const double half_width = config.half_width + config.safety_margin;
    const double offset = (config.front_extent - config.rear_extent) / 2;
    const double dx = x - (pose.x + offset * c), dy = y - (pose.y + offset * s);
    return std::abs(dx) <= half_cell + half_length * std::abs(c) + half_width * std::abs(s) &&
           std::abs(dy) <= half_cell + half_length * std::abs(s) + half_width * std::abs(c) &&
           std::abs(dx * c + dy * s) <= half_length + half_cell * (std::abs(c) + std::abs(s)) &&
           std::abs(-dx * s + dy * c) <= half_width + half_cell * (std::abs(c) + std::abs(s));
}
} // namespace

int main() {
    const Pose start{0.0, 0.0, 0.0};
    const Pose goal{20.0, 0.0, 0.0};
    Config config;
    auto grid = openGrid();

    const auto straight = plan(grid, start, goal, config);
    validPath(straight, start, goal, config);
    for (const auto& point : straight.path) {
        check(std::abs(point.pose.y) < 1.0, "open straight route should not wander");
    }

    // A physical obstacle intersects the straight path, but leaves room to pass.
    fill(grid, 9.0, 10.0, -0.5, 0.5, 100);
    const auto detour = plan(grid, start, goal, config);
    validPath(detour, start, goal, config);
    Config unsmoothed_config = config;
    unsmoothed_config.smooth_path = false;
    const auto raw_detour = plan(grid, start, goal, unsmoothed_config);
    validPath(raw_detour, start, goal, unsmoothed_config);
    const auto steering_changes = [](const Result& path) {
        double sum = 0.0, previous = 0.0;
        for (const auto& point : path.path) {
            const double change = point.steering - previous;
            sum += change * change;
            previous = point.steering;
        }
        return sum;
    };
    check(detour.smoothing_applied, "detour should admit a smoother steering sequence");
    check(steering_changes(detour) < steering_changes(raw_detour),
          "smoothing should reduce squared steering changes");
    bool moved_aside = false;
    for (const auto& point : detour.path) {
        moved_aside = moved_aside || std::abs(point.pose.y) > 1.5;
        for (std::size_t row = 0; row < grid.height; ++row) {
            for (std::size_t col = 0; col < grid.width; ++col) {
                if (grid.cells[row * grid.width + col] < config.occupied_threshold) continue;
                const double x = grid.origin_x + (col + 0.5) * grid.resolution;
                const double y = grid.origin_y + (row + 0.5) * grid.resolution;
                check(!overlapsCell(point.pose, config, x, y, grid.resolution / 2),
                      "detour vehicle footprint intersects an occupied cell");
            }
        }
    }
    check(moved_aside, "new obstacle should cause the replanned route to move aside");

    // A newly observed wall invalidates the previous path. Unknown cells must not
    // be interpreted as free, and failure must be delivered to the consumer.
    fill(grid, 6.0, 6.5, -10.0, 10.0, -1);
    expectFailure(plan(grid, start, goal, config), "unknown wall on updated map");

    auto footprint = openGrid();
    // The rear axle is clear, but a cell under the vehicle's front is occupied.
    fill(footprint, 1.0, 1.5, -0.5, 0.0, 100);
    expectFailure(plan(footprint, start, goal, config), "obstacle under front overhang");

    auto boundary = openGrid();
    // The reference point is inside the map, while the rear body extends beyond it.
    const Pose at_edge{boundary.origin_x + 0.1, 0.0, 0.0};
    expectFailure(plan(boundary, at_edge, goal, config), "vehicle extending beyond map boundary");

    grid = openGrid();
    const auto restored = plan(grid, start, goal, config);
    validPath(restored, start, goal, config);

    auto heading_config = config;
    heading_config.require_goal_yaw = true;
    const auto heading_path = plan(grid, start, goal, heading_config);
    validPath(heading_path, start, goal, heading_config);
    check(std::abs(std::remainder(heading_path.path.back().pose.yaw - goal.yaw,
                                  2.0 * std::acos(-1.0))) <= heading_config.goal_yaw_tolerance + 1e-6,
          "heading-constrained result misses the requested yaw");

    auto malformed = grid;
    malformed.cells.pop_back();
    expectFailure(plan(malformed, start, goal, config), "incorrect grid size");
    malformed = grid;
    malformed.resolution = std::numeric_limits<double>::quiet_NaN();
    expectFailure(plan(malformed, start, goal, config), "non-finite grid resolution");
    auto bad_config = config;
    bad_config.wheelbase = 0;
    expectFailure(plan(grid, start, goal, bad_config), "zero wheelbase");
    bad_config = config;
    bad_config.max_steering = std::numeric_limits<double>::quiet_NaN();
    expectFailure(plan(grid, start, goal, bad_config), "non-finite steering limit");
    auto bad_pose = start;
    bad_pose.x = std::numeric_limits<double>::quiet_NaN();
    expectFailure(plan(grid, bad_pose, goal, config), "non-finite start pose");
    std::cout << "Planner integration tests passed\n";
}
