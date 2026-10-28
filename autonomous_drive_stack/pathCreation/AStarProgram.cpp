#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace path_planning {

// A stable cell index supplied by the application's occupancy grid map.
using CellId = std::size_t;

// The adapter defines the heading resolution and maps orientations to canonical
// bins over one full turn (so 0 and 360 degrees use the same bin).
struct SearchState {
  CellId cell;
  std::size_t heading_bin;

  bool operator==(const SearchState& other) const {
    return cell == other.cell && heading_bin == other.heading_bin;
  }

  bool operator!=(const SearchState& other) const {
    return !(*this == other);
  }
};

struct SearchStateHash {
  std::size_t operator()(const SearchState& state) const {
    const std::size_t cell_hash = std::hash<CellId>{}(state.cell);
    const std::size_t heading_hash =
        std::hash<std::size_t>{}(state.heading_bin);
    return cell_hash ^ (heading_hash + 0x9e3779b9U +
                        (cell_hash << 6) + (cell_hash >> 2));
  }
};

// Steering angles are in degrees: positive is left, negative is right.
constexpr double kMaxSteerLeftDegrees = 25.0;
constexpr double kMaxSteerRightDegrees = -25.0;
constexpr double kForwardSteerDegrees = 0.0;
constexpr std::array<double, 3> kSteeringAnglesDegrees{
    kMaxSteerLeftDegrees, kForwardSteerDegrees, kMaxSteerRightDegrees};

// Implement this adapter around the application's existing occupancy grid.
// Map storage and motion generation are deliberately left to the adapter.
class OccupancyGridMap {
 public:
  virtual ~OccupancyGridMap() = default;

  // Set `next` to the cell and heading reached by a forward move from `from`
  // at this steering angle. Heading is vehicle orientation, not steering angle;
  // the adapter determines its change using the application's motion model.
  // Return false if the move is blocked or leaves the map. The adapter must
  // check the entire move for occupancy, not only its destination cell.
  virtual bool nextState(const SearchState& from,
                         double steering_angle_degrees,
                         SearchState& next) const = 0;

  // Return the non-negative cost of moving between connected states.
  // Positive infinity may be returned for a blocked transition.
  virtual double transitionCost(const SearchState& from,
                                const SearchState& to) const = 0;

  // Return a non-negative lower bound on the remaining cost. This must not
  // overestimate the true cost if an optimal result is required. Returning
  // zero is valid and makes the search equivalent to Dijkstra's algorithm.
  virtual double estimatedRemainingCost(const SearchState& from,
                                        const SearchState& goal) const = 0;
};

struct AStarResult {
  bool found = false;
  double total_cost = std::numeric_limits<double>::infinity();
  std::vector<SearchState> states;
};

namespace {

struct OpenEntry {
  SearchState state;
  double cost_from_start;
  double estimated_total_cost;
};

struct LowestEstimatedCostFirst {
  bool operator()(const OpenEntry& left, const OpenEntry& right) const {
    return left.estimated_total_cost > right.estimated_total_cost;
  }
};

double checkedHeuristic(const OccupancyGridMap& grid,
                        const SearchState& from,
                        const SearchState& goal) {
  const double estimate = grid.estimatedRemainingCost(from, goal);
  if (!std::isfinite(estimate) || estimate < 0.0) {
    throw std::invalid_argument(
        "A* heuristic must be finite and non-negative");
  }
  return estimate;
}

std::vector<SearchState> reconstructPath(
    const SearchState& start,
    const SearchState& goal,
    const std::unordered_map<SearchState, SearchState, SearchStateHash>&
        came_from) {
  std::vector<SearchState> path{goal};
  SearchState current = goal;

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

// Searches an existing occupancy grid using the three steering choices above.
// Each cell/heading-bin pair is a distinct state, including at the goal.
// Vehicle motion modeling remains the adapter's responsibility.
// The returned path contains both `start` and `goal`.
// If no path exists, `found` is false.
AStarResult findPath(const OccupancyGridMap& grid,
                     SearchState start,
                     SearchState goal) {
  if (start == goal) {
    return {true, 0.0, {start}};
  }

  std::priority_queue<OpenEntry,
                      std::vector<OpenEntry>,
                      LowestEstimatedCostFirst>
      open;
  std::unordered_map<SearchState, double, SearchStateHash> cost_from_start;
  std::unordered_map<SearchState, SearchState, SearchStateHash> came_from;

  cost_from_start.emplace(start, 0.0);
  open.push({start, 0.0, checkedHeuristic(grid, start, goal)});

  while (!open.empty()) {
    const OpenEntry current = open.top();
    open.pop();

    const auto best_known = cost_from_start.find(current.state);
    if (best_known == cost_from_start.end() ||
        current.cost_from_start > best_known->second) {
      continue;  // This queue entry was superseded by a cheaper route.
    }

    if (current.state == goal) {
      return {true,
              current.cost_from_start,
              reconstructPath(start, goal, came_from)};
    }

    for (const double steering_angle_degrees : kSteeringAnglesDegrees) {
      SearchState neighbor{};
      if (!grid.nextState(current.state, steering_angle_degrees, neighbor)) {
        continue;
      }
      const double edge_cost =
          grid.transitionCost(current.state, neighbor);
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
      came_from[neighbor] = current.state;
      open.push({neighbor,
                 candidate_cost,
                 candidate_cost + checkedHeuristic(grid, neighbor, goal)});
    }
  }

  return {};
}

}  // namespace path_planning

