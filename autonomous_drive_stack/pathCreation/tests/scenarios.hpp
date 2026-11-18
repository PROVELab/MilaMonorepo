#pragma once

// Shared spatial inputs for the tests and the standalone visualizer.
#include "prove/grid_builder.hpp"

#include <cmath>

namespace prove::planning::scenarios {

inline OccupancyGrid openGrid() {
    OccupancyGrid grid;
    grid.width = 70;
    grid.height = 40;
    grid.resolution = 0.5;
    grid.origin_x = -5.0;
    grid.origin_y = -10.0;
    grid.cells.assign(grid.width * grid.height, 0);
    return grid;
}

inline void fill(OccupancyGrid& grid, double min_x, double max_x, double min_y,
                 double max_y, std::int8_t value) {
    for (std::size_t row = 0; row < grid.height; ++row) {
        for (std::size_t col = 0; col < grid.width; ++col) {
            const double x = grid.origin_x + (col + 0.5) * grid.resolution;
            const double y = grid.origin_y + (row + 0.5) * grid.resolution;
            if (x >= min_x && x < max_x && y >= min_y && y < max_y)
                grid.cells[row * grid.width + col] = value;
        }
    }
}

inline const Pose start{0, 0, 0}, goal{20, 0, 0};
inline void addDetourObstacle(OccupancyGrid& grid) { fill(grid, 9, 10, -0.5, 0.5, 100); }
inline void addUnknownWall(OccupancyGrid& grid) { fill(grid, 6, 6.5, -10, 10, -1); }
inline void addFrontObstacle(OccupancyGrid& grid) { fill(grid, 1, 1.5, -0.5, 0, 100); }

inline const GridGeometry geometry{10, 10, 1, 0, 0};
inline const Polygon area{{1.5,1.5}, {8.5,1.5}, {8.5,8.5}, {1.5,8.5}, {1.5,1.5}};
inline const std::vector<CircleObstacle> circles{{{5.5, 5.5}, 0.5}};
inline const Polygon obstacle{{7.2,3.2}, {7.8,3.2}, {7.8,3.8}, {7.2,3.8}, {7.2,3.2}};
inline const BoxObstacle box{{4.5,4.5}, 2, 0.3, std::acos(-1.0) / 2};
inline const std::vector<Point2> left{{2,8}, {5,8}, {8,8}}, right{{2,2}, {5,2}, {8,2}};

inline const GridGeometry road_geometry{80, 40, 0.5, 0, 0};
inline const Polygon road{{0,0}, {40,0}, {40,20}, {0,20}, {0,0}};
inline const Pose road_start{5,10,0}, road_goal{30,10,0};
inline const std::vector<CircleObstacle> road_circles{{{15,10}, 0.75}};
inline const Polygon wall{{13,0}, {14,0}, {14,20}, {13,20}, {13,0}};

// Synthetic static road scenes, in meters. Dimensions are illustrative rather
// than measured vehicle/sensor data. Both tests and viewer consume these objects.
struct StaticScenario {
    std::string name, description;
    GridGeometry geometry;
    Polygon boundary;
    std::vector<Point2> left, right;
    std::vector<CircleObstacle> circles;
    std::vector<Polygon> polygons;
    std::vector<BoxObstacle> boxes;
    Pose start, goal;
    Config config;
    bool expected_success = true;
};

inline std::vector<StaticScenario> staticScenarios() {
    std::vector<StaticScenario> cases;
    StaticScenario s;
    s.name = "Static / staggered parked cars";
    s.description = "Two stationary 4.5 m cars intrude from opposite sides, requiring successive steering changes. Vehicle dimensions are illustrative.";
    s.geometry = {180, 64, 0.25, 0, 0};
    s.boundary = {{0,0}, {45,0}, {45,16}, {0,16}, {0,0}};
    s.start = {4,8,0}; s.goal = {39,8,0};
    s.boxes = {{{13,6.5}, 4.5, 1.8, 0.08}, {{26,9.5}, 4.5, 1.8, -0.08}};
    cases.push_back(s);

    s = StaticScenario{};
    s.name = "Static / construction clutter";
    s.description = "A stationary drum, angled barrier, and irregular debris polygon share a bounded road. Physical geometry is rasterized before planning.";
    s.geometry = {160, 72, 0.25, 0, 0};
    s.boundary = {{0,0}, {40,0}, {40,18}, {0,18}, {0,0}};
    s.start = {4,9,0}; s.goal = {35,9,0};
    s.circles = {{{13,9}, 0.65}};
    s.boxes = {{{25,8}, 5, 1.8, -0.25}};
    s.polygons = {{{17,13}, {21,13}, {20,16}, {18,15}, {17,13}}};
    cases.push_back(s);

    s = StaticScenario{};
    s.name = "Static / narrow gate";
    s.description = "Offset approach and exit through a 3.5 m gap between static barriers. The full body and safety margin must fit.";
    s.geometry = {160, 64, 0.25, 0, 0};
    s.boundary = {{0,0}, {40,0}, {40,16}, {0,16}, {0,0}};
    s.start = {4,6,0}; s.goal = {35,10,0};
    s.boxes = {{{18,3.125}, 1, 6.25, 0}, {{18,12.875}, 1, 6.25, 0}};
    cases.push_back(s);

    s = StaticScenario{};
    s.name = "Lanes / S bend with static drum";
    s.description = "Ordered lane edges bend around an S-shaped corridor. A stationary drum requires an in-corridor detour; crossing the lane edges is blocked.";
    s.geometry = {160, 64, 0.25, 0, 0};
    s.left = {{0,8.5}, {8,8.5}, {16,12.5}, {24,12.5}, {32,8.5}, {40,8.5}};
    s.right = {{0,1.5}, {8,1.5}, {16,5.5}, {24,5.5}, {32,1.5}, {40,1.5}};
    s.start = {4,5,0}; s.goal = {35,5,0};
    s.circles = {{{21,9}, 0.4}};
    cases.push_back(s);

    s.name = "Lanes / blocked S bend";
    s.description = "The same lane corridor with a stationary barrier spanning both boundaries. The planner must fail rather than leave the observed lane.";
    s.circles.clear();
    s.boxes = {{{20,9}, 1, 8, 0}};
    s.config.max_expansions = 30000;
    s.expected_success = false;
    cases.push_back(s);
    return cases;
}

} // namespace prove::planning::scenarios
