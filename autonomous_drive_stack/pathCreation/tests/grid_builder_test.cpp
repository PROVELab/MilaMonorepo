#include "prove/grid_builder.hpp"

#include <cmath>
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
int cell(const OccupancyGrid& grid, std::size_t x, std::size_t y) {
    return grid.cells[y * grid.width + x];
}
void failed(const GridBuildResult& result, const std::string& message) {
    check(!result.success, message + " must fail");
    check(!result.reason.empty(), message + " must have a reason");
    check(result.grid.cells.empty(), message + " must not return a usable grid");
}
}

int main() {
    const GridGeometry geometry{10, 10, 1.0, 0.0, 0.0};
    const Polygon area{{1.5,1.5}, {8.5,1.5}, {8.5,8.5}, {1.5,8.5}, {1.5,1.5}};
    auto map = buildOccupancyGrid(geometry, area);
    check(map.success, "closed area should build: " + map.reason);
    check(map.grid.width == 10 && map.grid.height == 10 && map.grid.cells.size() == 100,
          "grid metadata and cell count");
    check(cell(map.grid, 5, 5) == 0, "interior must be free");
    check(cell(map.grid, 0, 4) < 0, "unobserved area must remain unknown");
    check(cell(map.grid, 1, 4) == 100, "cell touching boundary must be blocked");
    check(cell(map.grid, 2, 4) == 0, "cell entirely inside area should be free");

    // Circle covers its center and also touches adjacent cells even though their
    // centers lie outside the radius. A thin rectangle lies within one cell.
    const std::vector<CircleObstacle> circles{{{5.5, 5.5}, 0.5}};
    const Polygon obstacle{{7.2, 3.2}, {7.8, 3.2}, {7.8, 3.8}, {7.2, 3.8}, {7.2, 3.2}};
    map = buildOccupancyGrid(geometry, area, circles, {obstacle});
    check(map.success, "valid obstacles should build: " + map.reason);
    check(cell(map.grid, 5, 5) == 100 && cell(map.grid, 4, 5) == 100 &&
          cell(map.grid, 6, 5) == 100, "circle size and touching cells must be blocked");
    check(cell(map.grid, 7, 3) == 100, "polygon inside a cell must be blocked");
    check(cell(map.grid, 3, 3) == 0, "unrelated interior must remain free");

    const BoxObstacle box{{4.5, 4.5}, 2.0, 0.3, std::acos(-1.0) / 2.0};
    map = buildOccupancyGrid(geometry, area, {}, {}, {box});
    check(map.success, "rotated box should build");
    check(cell(map.grid, 4, 3) == 100 && cell(map.grid, 4, 5) == 100,
          "rotated box must occupy cells along its long axis");
    check(cell(map.grid, 3, 4) == 0 && cell(map.grid, 5, 4) == 0,
          "rotated box must preserve clearance beyond its short axis");
    failed(buildOccupancyGrid(geometry, area, {}, {}, {{{4,4}, 0, 1, 0}}), "zero-size box");

    const auto corridor = closeCorridor({{2,8}, {5,8}, {8,8}}, {{2,2}, {5,2}, {8,2}});
    check(corridor.success && !corridor.polygon.empty(), "ordered lane edges should form a corridor");
    map = buildOccupancyGrid(geometry, corridor.polygon);
    check(map.success && cell(map.grid, 4, 4) == 0 && cell(map.grid, 4, 1) != 0,
          "corridor must include interior but not space outside its edges");
    const auto crossed = closeCorridor({{2,8}, {8,8}}, {{8,2}, {2,2}});
    check(!crossed.success && crossed.polygon.empty(), "oppositely ordered edges must fail when caps cross");

    auto bad_area = area;
    bad_area.pop_back();
    failed(buildOccupancyGrid(geometry, bad_area), "unclosed boundary");
    bad_area = {{1,1}, {9,9}, {1,9}, {9,1}, {1,1}};
    failed(buildOccupancyGrid(geometry, bad_area), "self-intersecting boundary");
    bad_area = area;
    bad_area[1].x = std::numeric_limits<double>::quiet_NaN();
    failed(buildOccupancyGrid(geometry, bad_area), "non-finite boundary");
    failed(buildOccupancyGrid(geometry, area, {{{5,5}, -1}}), "negative obstacle radius");
    failed(buildOccupancyGrid(geometry, area, {}, {{{2,2}, {3,2}, {3,3}}}), "unclosed obstacle");
    auto bad_geometry = geometry;
    bad_geometry.resolution = 0;
    failed(buildOccupancyGrid(bad_geometry, area), "zero resolution");

    // Exercise the perception geometry -> grid -> planner handoff on successive
    // snapshots: open route, an obstacle requiring a detour, then a solid wall.
    const GridGeometry road_geometry{80, 40, 0.5, 0, 0};
    const Polygon road{{0,0}, {40,0}, {40,20}, {0,20}, {0,0}};
    const Pose start{5,10,0}, goal{30,10,0};
    Config config;
    auto open = buildOccupancyGrid(road_geometry, road);
    check(open.success, "open road grid should build");
    auto open_path = plan(open.grid, start, goal, config);
    check(open_path.success && !open_path.path.empty(), "open road should yield a path");
    auto obstacle_map = buildOccupancyGrid(road_geometry, road, {{{15,10}, 0.75}});
    check(obstacle_map.success, "obstacle road grid should build");
    auto detour = plan(obstacle_map.grid, start, goal, config);
    check(detour.success && !detour.path.empty(), "obstacle should admit a detour");
    bool moved_aside = false;
    for (const auto& point : detour.path) moved_aside |= std::abs(point.pose.y - 10) > 2.0;
    check(moved_aside, "new obstacle must change the route");
    const Polygon wall{{13,0}, {14,0}, {14,20}, {13,20}, {13,0}};
    auto blocked = buildOccupancyGrid(road_geometry, road, {}, {wall});
    check(blocked.success, "wall road grid should build");
    config.max_expansions = 15000;
    auto no_path = plan(blocked.grid, start, goal, config);
    check(!no_path.success && no_path.path.empty(), "blocking wall must invalidate the path");
    std::cout << "Grid builder tests passed\n";
}
