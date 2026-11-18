#pragma once

#include "prove/planner.hpp"

#include <limits>

namespace prove::planning {

struct Point2 { double x = 0.0, y = 0.0; };
// Vertices follow the perimeter in order; the last point must equal the first.
using Polygon = std::vector<Point2>;
struct CircleObstacle { Point2 center; double radius = 0.0; };
// Yaw is the known orientation of the length axis in the common map frame.
// If orientation is unknown, use a circle enclosing the whole object instead.
struct BoxObstacle {
    Point2 center;
    double length = 0.0, width = 0.0;
    double yaw = std::numeric_limits<double>::quiet_NaN();
};

struct CorridorResult {
    bool success = false;
    std::string reason;
    Polygon polygon;
};

// Both edges must be ordered in the same travel direction. The two end caps
// connect the corresponding endpoints; no extension is made beyond them.
CorridorResult closeCorridor(const std::vector<Point2>& left, const std::vector<Point2>& right);

// The lower-left corner of the axis-aligned map and cell dimensions in meters.
// All geometry, the map, and the planner poses must use the same frame.
struct GridGeometry {
    std::size_t width = 0, height = 0;
    double resolution = 0.0, origin_x = 0.0, origin_y = 0.0;
};

struct GridBuildResult {
    bool success = false;
    std::string reason;
    OccupancyGrid grid;
};

// Free cells are entirely inside drivable_area and do not touch an obstacle.
// The entire polygon is assumed observed, with a complete obstacle list;
// geometry alone cannot identify unknown or occluded areas inside it. Mark such
// cells unknown after building, before passing the grid to the planner. Cells
// outside are unknown (-1); cells touching boundaries/obstacles are 100.
// Obstacles describe physical size and must not
// include vehicle inflation; the planner handles vehicle size and margin.
// A failed build returns an empty grid. Inputs must be a complete observation
// of the chosen drivable polygon, not just isolated lines or detected points.
GridBuildResult buildOccupancyGrid(const GridGeometry& geometry,
                                   const Polygon& drivable_area,
                                   const std::vector<CircleObstacle>& circles = {},
                                   const std::vector<Polygon>& polygons = {},
                                   const std::vector<BoxObstacle>& boxes = {});

}  // namespace prove::planning
