#include "prove/grid_builder.hpp"

#include <iostream>
#include <vector>

using namespace prove::planning;

// A controls adapter should replace its current path on every result, including
// clearing it on failure. A real integration must reject stale snapshots/results
// and stop safely when updates are missing.
void send_to_controls(const Result& result, std::vector<PathPoint>& active_path) {
    if (!result.success) {
        active_path.clear();
        std::cout << "No usable path; controls must stop: " << result.reason << '\n';
        return;
    }
    active_path = result.path;
    std::cout << "New path: " << result.path.size() << " points, cost "
              << result.cost << '\n';
}

int main() {
    // Example inputs: both boundaries run in the same travel direction, and
    // everything inside them is known to have been observed. Real perception
    // must supply complete, synchronized geometry in the same frame as poses.
    const std::vector<Point2> left{{0.0, 19.0}, {40.0, 19.0}};
    const std::vector<Point2> right{{0.0, 1.0}, {40.0, 1.0}};
    const GridGeometry geometry{80, 40, 0.5, 0.0, 0.0};
    const Pose current{5.0, 10.0, 0.0}; // Rear axle, in the map's frame.
    const Pose goal{32.0, 10.0, 0.0};
    Config config;
    config.max_expansions = 10000;
    std::vector<PathPoint> active_path;

    // In production this loop is driven by fresh perception and localization
    // snapshots. Here it illustrates an obstacle appearing, a blocking wall,
    // invalid obstacle and boundary inputs, and recovery. Dimensions describe the physical obstacles;
    // the planner separately accounts for the vehicle footprint and margin.
    for (int snapshot = 0; snapshot < 6; ++snapshot) {
        std::vector<CircleObstacle> circles;
        std::vector<BoxObstacle> boxes;
        if (snapshot == 1) boxes.push_back({{18.0, 10.0}, 1.0, 1.0, 0.0});
        if (snapshot == 2) boxes.push_back({{20.0, 10.0}, 1.0, 18.0, 0.0});
        if (snapshot == 3) boxes.push_back({{18.0, 10.0}, 0.0, 1.0, 0.0});

        // Perception has not provided a valid left boundary in snapshot 4.
        const auto corridor = closeCorridor(snapshot == 4 ? std::vector<Point2>{} : left, right);
        if (!corridor.success) {
            Result failure;
            failure.reason = "invalid boundary: " + corridor.reason;
            send_to_controls(failure, active_path);
            continue;
        }
        const auto map = buildOccupancyGrid(geometry, corridor.polygon, circles, {}, boxes);
        if (!map.success) {
            Result failure;
            failure.reason = "grid build failed: " + map.reason;
            send_to_controls(failure, active_path);
            continue;
        }
        send_to_controls(plan(map.grid, current, goal, config), active_path);
    }
}
