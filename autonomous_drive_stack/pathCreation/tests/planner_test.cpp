#include "scenarios.hpp"

#include <algorithm>
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

using scenarios::openGrid;

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

// Check the inflated body independently of the planner's private collision code.
// Includes unknown space and map edges, not just the physical obstacles.
bool clearFootprint(const OccupancyGrid& grid, Pose pose, const Config& config) {
    const double c = std::cos(pose.yaw), s = std::sin(pose.yaw);
    for (double x : {-config.rear_extent - config.safety_margin,
                      config.front_extent + config.safety_margin}) {
        for (double y : {-config.half_width - config.safety_margin,
                          config.half_width + config.safety_margin}) {
            const double wx = pose.x + x * c - y * s, wy = pose.y + x * s + y * c;
            if (wx <= grid.origin_x || wy <= grid.origin_y ||
                wx >= grid.origin_x + grid.width * grid.resolution ||
                wy >= grid.origin_y + grid.height * grid.resolution) return false;
        }
    }
    const double radius = std::hypot(std::max(config.front_extent, config.rear_extent) + config.safety_margin,
                                     config.half_width + config.safety_margin);
    const int x0 = std::max(0, static_cast<int>(std::floor((pose.x - radius - grid.origin_x) / grid.resolution)) - 1);
    const int y0 = std::max(0, static_cast<int>(std::floor((pose.y - radius - grid.origin_y) / grid.resolution)) - 1);
    const int x1 = std::min(static_cast<int>(grid.width) - 1,
                          static_cast<int>(std::floor((pose.x + radius - grid.origin_x) / grid.resolution)) + 1);
    const int y1 = std::min(static_cast<int>(grid.height) - 1,
                          static_cast<int>(std::floor((pose.y + radius - grid.origin_y) / grid.resolution)) + 1);
    for (int row = y0; row <= y1; ++row) {
        for (int col = x0; col <= x1; ++col) {
            const int value = grid.cells[row * grid.width + col];
            if (value >= 0 && value < config.occupied_threshold) continue;
            if (overlapsCell(pose, config, grid.origin_x + (col + 0.5) * grid.resolution,
                             grid.origin_y + (row + 0.5) * grid.resolution, grid.resolution / 2)) return false;
        }
    }
    return true;
}

void staticScenario(std::size_t index) {
    const auto cases = scenarios::staticScenarios();
    check(index < cases.size(), "unknown static scenario");
    const auto& scene = cases[index];
    std::cout << "Testing " << scene.name << std::endl;
    auto boundary = scene.boundary;
    if (!scene.left.empty()) {
        const auto lane = closeCorridor(scene.left, scene.right);
        check(lane.success, scene.name + ": lane edges must form a valid corridor");
        boundary = lane.polygon;
    }
    const auto map = buildOccupancyGrid(scene.geometry, boundary, scene.circles, scene.polygons, scene.boxes);
    check(map.success, scene.name + ": map build failed: " + map.reason);
    check(clearFootprint(map.grid, scene.start, scene.config), scene.name + ": start must be clear");
    check(clearFootprint(map.grid, scene.goal, scene.config), scene.name + ": goal must be clear");
    const auto result = plan(map.grid, scene.start, scene.goal, scene.config);
    if (!scene.expected_success) {
        expectFailure(result, scene.name);
        check(result.expanded > 0, scene.name + ": must exercise search, not reject an invalid input");
        return;
    }
    check(result.success, scene.name + ": " + result.reason);
    validPath(result, scene.start, scene.goal, scene.config);
    for (std::size_t i = 0; i < result.path.size(); ++i) {
        const auto& point = result.path[i];
        check(clearFootprint(map.grid, point.pose, scene.config),
              scene.name + ": inflated body touches an obstacle, unknown cell, or lane boundary");
        if (!i) continue;
        // Probe intermediate poses on each reported bicycle arc at <= 5 cm.
        // This is a dense regression check, not a continuous collision proof.
        const auto previous = result.path[i - 1].pose;
        const double yaw_delta = std::remainder(point.pose.yaw - previous.yaw, 2 * std::acos(-1.0));
        const double length = std::abs(point.curvature) > 1e-8 ? yaw_delta / point.curvature :
                              std::hypot(point.pose.x - previous.x, point.pose.y - previous.y);
        const int samples = std::max(1, static_cast<int>(std::ceil(length / 0.05)));
        for (int j = 1; j < samples; ++j) {
            const double distance = length * j / samples;
            Pose p{previous.x + distance * std::cos(previous.yaw),
                   previous.y + distance * std::sin(previous.yaw), previous.yaw};
            if (std::abs(point.curvature) > 1e-8) {
                p.yaw += distance * point.curvature;
                p.x = previous.x + (std::sin(p.yaw) - std::sin(previous.yaw)) / point.curvature;
                p.y = previous.y + (std::cos(previous.yaw) - std::cos(p.yaw)) / point.curvature;
            }
            check(clearFootprint(map.grid, p, scene.config), scene.name + ": collision between returned samples");
        }
    }
    // Verify the intended maneuvers actually occurred, without requiring an
    // exact waypoint sequence or one particular side for unconstrained detours.
    if (index == 0) {
        bool passed_above = false, passed_below = false;
        for (const auto& p : result.path) {
            passed_above |= std::abs(p.pose.x - 13) < 0.5 && p.pose.y > 8.5;
            passed_below |= std::abs(p.pose.x - 26) < 0.5 && p.pose.y < 7.5;
        }
        check(passed_above && passed_below, scene.name + ": must weave between both parked cars");
    }
    if (!scene.left.empty()) {
        bool followed_bend = false;
        for (const auto& p : result.path) followed_bend |= p.pose.x > 17 && p.pose.x < 23 && p.pose.y > 7;
        check(followed_bend, scene.name + ": must follow the lane bend");
        // Confirm that an apparently convenient straight shortcut is unavailable.
        check(!clearFootprint(map.grid, {20,5,0}, scene.config), scene.name + ": outside-lane shortcut must be blocked");
    }
    std::cout << scene.name << " passed (" << result.expanded << " expansions)\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--static") {
        char* end = nullptr;
        const long index = std::strtol(argv[2], &end, 10);
        check(end != argv[2] && *end == '\0' && index >= 0, "invalid static scenario index");
        staticScenario(static_cast<std::size_t>(index));
        return 0;
    }
    check(argc == 1, "usage: planner_test [--static index]");
    const auto start = scenarios::start;
    const auto goal = scenarios::goal;
    Config config;
    auto grid = openGrid();

    const auto straight = plan(grid, start, goal, config);
    validPath(straight, start, goal, config);
    for (const auto& point : straight.path) {
        check(std::abs(point.pose.y) < 1.0, "open straight route should not wander");
    }

    // A physical obstacle intersects the straight path, but leaves room to pass.
    scenarios::addDetourObstacle(grid);
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
    scenarios::addUnknownWall(grid);
    expectFailure(plan(grid, start, goal, config), "unknown wall on updated map");

    auto footprint = openGrid();
    // The rear axle is clear, but a cell under the vehicle's front is occupied.
    scenarios::addFrontObstacle(footprint);
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
