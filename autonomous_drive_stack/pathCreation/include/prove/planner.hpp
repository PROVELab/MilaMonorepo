#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace prove::planning {

// All distances are meters and all angles are radians. Pose is the rear axle.
struct Pose { double x = 0.0, y = 0.0, yaw = 0.0; };

// Axis-aligned, row-major grid in the same frame as the poses. The origin is
// the lower-left corner of cell (0,0). Negative values are unknown and blocked.
struct OccupancyGrid {
    std::size_t width = 0, height = 0;
    double resolution = 0.0, origin_x = 0.0, origin_y = 0.0;
    std::vector<std::int8_t> cells;
};

struct Config {
    double wheelbase = 1.6;
    double max_steering = 0.42;
    // Rectangle relative to the rear axle, including overhangs.
    double front_extent = 2.0, rear_extent = 0.8, half_width = 0.75;
    double safety_margin = 0.35;
    double primitive_length = 0.75;
    double goal_tolerance = 0.8, goal_yaw_tolerance = 0.35;
    double steering_penalty = 0.04, steering_change_penalty = 0.3;
    int heading_bins = 72, max_expansions = 100000, occupied_threshold = 50;
    bool require_goal_yaw = false;
    bool smooth_path = true;
};

// Steering and curvature describe the incoming arc ending at this point.
struct PathPoint { Pose pose; double steering = 0.0, curvature = 0.0; };
struct Result {
    bool success = false;
    std::string reason;
    int expanded = 0;
    double cost = 0.0; // Hybrid A* search cost before smoothing.
    bool smoothing_applied = false;
    std::vector<PathPoint> path;
};

// Forward-only Hybrid A*. An unsuccessful result always has an empty path.
Result plan(const OccupancyGrid& grid, Pose start, Pose goal, const Config& config = {});

}  // namespace prove::planning
