#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace path_planning {

// A stable index supplied by the imported Delaunay-triangulation library.
using TriangleId = std::size_t;

// Implement this adapter around the Delaunay library used by the application.
// The adapter reads an existing triangulation; AStar never creates triangles.
class DelaunayTriangleGrid {
 public:
  virtual ~DelaunayTriangleGrid() = default;

  // Return triangles that share a traversable edge with `triangle`.
  virtual std::vector<TriangleId> neighbors(TriangleId triangle) const = 0;

  // Return the non-negative cost of moving between adjacent triangles.
  // Positive infinity may be returned for a blocked transition.
  virtual double transitionCost(TriangleId from, TriangleId to) const = 0;

  // Return a non-negative lower bound on the remaining cost. This must not
  // overestimate the true cost if an optimal result is required. Returning
  // zero is valid and makes the search equivalent to Dijkstra's algorithm.
  virtual double estimatedRemainingCost(TriangleId from,
                                        TriangleId goal) const = 0;
};

struct AStarResult {
  bool found = false;
  double total_cost = std::numeric_limits<double>::infinity();
  std::vector<TriangleId> triangles;
};

namespace {

struct OpenEntry {
  TriangleId triangle;
  double cost_from_start;
  double estimated_total_cost;
};

struct LowestEstimatedCostFirst {
  bool operator()(const OpenEntry& left, const OpenEntry& right) const {
    return left.estimated_total_cost > right.estimated_total_cost;
  }
};

double checkedHeuristic(const DelaunayTriangleGrid& grid,
                        TriangleId from,
                        TriangleId goal) {
  const double estimate = grid.estimatedRemainingCost(from, goal);
  if (!std::isfinite(estimate) || estimate < 0.0) {
    throw std::invalid_argument(
        "A* heuristic must be finite and non-negative");
  }
  return estimate;
}

std::vector<TriangleId> reconstructPath(
    TriangleId start,
    TriangleId goal,
    const std::unordered_map<TriangleId, TriangleId>& came_from) {
  std::vector<TriangleId> path{goal};
  TriangleId current = goal;

  while (current != start) {
    const auto parent = came_from.find(current);
    if (parent == came_from.end()) {
      throw std::logic_error("A* predecessor chain is incomplete");
    }
    current = parent->second;
    path.push_back(current);
  }

  std::reverse(path.begin(), path.end());
  return path;
}

}  // namespace

// Searches an already-built Delaunay triangle adjacency graph. The returned
// path contains both `start` and `goal`. If no path exists, `found` is false.
AStarResult findPath(const DelaunayTriangleGrid& grid,
                     TriangleId start,
                     TriangleId goal) {
  if (start == goal) {
    return {true, 0.0, {start}};
  }

  std::priority_queue<OpenEntry,
                      std::vector<OpenEntry>,
                      LowestEstimatedCostFirst>
      open;
  std::unordered_map<TriangleId, double> cost_from_start;
  std::unordered_map<TriangleId, TriangleId> came_from;

  cost_from_start.emplace(start, 0.0);
  open.push({start, 0.0, checkedHeuristic(grid, start, goal)});

  while (!open.empty()) {
    const OpenEntry current = open.top();
    open.pop();

    const auto best_known = cost_from_start.find(current.triangle);
    if (best_known == cost_from_start.end() ||
        current.cost_from_start > best_known->second) {
      continue;  // This queue entry was superseded by a cheaper route.
    }

    if (current.triangle == goal) {
      return {true,
              current.cost_from_start,
              reconstructPath(start, goal, came_from)};
    }

    for (const TriangleId neighbor : grid.neighbors(current.triangle)) {
      const double edge_cost =
          grid.transitionCost(current.triangle, neighbor);
      if (std::isinf(edge_cost) && edge_cost > 0.0) {
        continue;
      }
      if (!std::isfinite(edge_cost) || edge_cost < 0.0) {
        throw std::invalid_argument(
            "A* transition costs must be non-negative or positive infinity");
      }

      const double candidate_cost = current.cost_from_start + edge_cost;
      const auto known_neighbor = cost_from_start.find(neighbor);
      if (known_neighbor != cost_from_start.end() &&
          candidate_cost >= known_neighbor->second) {
        continue;
      }

      cost_from_start[neighbor] = candidate_cost;
      came_from[neighbor] = current.triangle;
      open.push({neighbor,
                 candidate_cost,
                 candidate_cost + checkedHeuristic(grid, neighbor, goal)});
    }
  }

  return {};
}

}  // namespace path_planning

