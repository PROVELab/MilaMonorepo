#include "scenarios.hpp"

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
    const auto& geometry = scenarios::geometry;
    const auto& area = scenarios::area;
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
    const auto& circles = scenarios::circles;
    const auto& obstacle = scenarios::obstacle;
    map = buildOccupancyGrid(geometry, area, circles, {obstacle});
    check(map.success, "valid obstacles should build: " + map.reason);
    check(cell(map.grid, 5, 5) == 100 && cell(map.grid, 4, 5) == 100 &&
          cell(map.grid, 6, 5) == 100, "circle size and touching cells must be blocked");
    check(cell(map.grid, 7, 3) == 100, "polygon inside a cell must be blocked");
    check(cell(map.grid, 3, 3) == 0, "unrelated interior must remain free");

    const auto& box = scenarios::box;
    map = buildOccupancyGrid(geometry, area, {}, {}, {box});
    check(map.success, "rotated box should build");
    check(cell(map.grid, 4, 3) == 100 && cell(map.grid, 4, 5) == 100,
          "rotated box must occupy cells along its long axis");
    check(cell(map.grid, 3, 4) == 0 && cell(map.grid, 5, 4) == 0,
          "rotated box must preserve clearance beyond its short axis");
    failed(buildOccupancyGrid(geometry, area, {}, {}, {{{4,4}, 0, 1, 0}}), "zero-size box");

    const auto corridor = closeCorridor(scenarios::left, scenarios::right);
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
    const auto& road_geometry = scenarios::road_geometry;
    const auto& road = scenarios::road;
    const auto start = scenarios::road_start, goal = scenarios::road_goal;
    Config config;
    auto open = buildOccupancyGrid(road_geometry, road);
    check(open.success, "open road grid should build");
    auto open_path = plan(open.grid, start, goal, config);
    check(open_path.success && !open_path.path.empty(), "open road should yield a path");
    auto obstacle_map = buildOccupancyGrid(road_geometry, road, scenarios::road_circles);
    check(obstacle_map.success, "obstacle road grid should build");
    auto detour = plan(obstacle_map.grid, start, goal, config);
    check(detour.success && !detour.path.empty(), "obstacle should admit a detour");
    bool moved_aside = false;
    for (const auto& point : detour.path) moved_aside |= std::abs(point.pose.y - 10) > 2.0;
    check(moved_aside, "new obstacle must change the route");
    const auto& wall = scenarios::wall;
    auto blocked = buildOccupancyGrid(road_geometry, road, {}, {wall});
    check(blocked.success, "wall road grid should build");
    config.max_expansions = 15000;
    auto no_path = plan(blocked.grid, start, goal, config);
    check(!no_path.success && no_path.path.empty(), "blocking wall must invalidate the path");
    std::cout << "Grid builder tests passed\n";
}
