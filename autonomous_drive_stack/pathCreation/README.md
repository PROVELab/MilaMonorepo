# Path creation

This directory contains a dependency-free C++17 occupancy grid builder and
Hybrid A* planner. The builder converts observed boundaries and obstacle shapes
into a grid; the planner converts that grid, the vehicle's current pose, and a
goal into a forward-only path. They are independent of the raylib simulator and
can be linked into the stack as `prove_planner`.

The stack does not yet have perception, localization, or controls interfaces.
The example in `examples/planner_updates.cpp` shows the handoff using successive
map snapshots; it does not connect to real sensors or command a vehicle.

## Build and try it

From the repository root:

Mac:
```sh
cmake -S autonomous_drive_stack/pathCreation -B /tmp/prove-planner-build
cmake --build /tmp/prove-planner-build
ctest --test-dir /tmp/prove-planner-build --output-on-failure
/tmp/prove-planner-build/planner_updates_example
```

If your on Windows: 
```sh
cmake -S autonomous_drive_stack/pathCreation -B build/prove-planner
cmake --build build/prove-planner --config Debug
ctest --test-dir build/prove-planner -C Debug --output-on-failure
.\build\prove-planner\Debug\planner_updates_example.exe
```

The standalone build does not need the Ouster SDK. The library is also added to
the main autonomy stack's CMake build.

## Visualize the test scenes

`examples/planner_visualizer.cpp` runs the real planner on the spatial test
scenarios and writes a single HTML file. It uses only the C++ standard library
and the existing planner; the viewer uses your browser's built-in canvas, with
no packages, network requests, or local server. Scene inputs are shared with
the tests in `tests/scenarios.hpp`.

From `MilaMonorepo` in Windows PowerShell (Visual Studio generator):

```powershell
cmake -S autonomous_drive_stack/pathCreation -B build/prove-planner
cmake --build build/prove-planner --config Debug
ctest --test-dir build/prove-planner -C Debug --output-on-failure
.\build\prove-planner\Debug\planner_visualizer.exe .\build\prove-planner\planner_visualizer.html
Start-Process .\build\prove-planner\planner_visualizer.html
```

With Ninja or Makefiles, the executable is directly inside `build/prove-planner`
instead of its `Debug` subfolder. On Linux/macOS, after the build above:

```sh
/tmp/prove-planner-build/planner_visualizer /tmp/planner_visualizer.html
# Open /tmp/planner_visualizer.html in your browser.
```

Choose a scene to inspect obstacles, unknown cells, detours, blocked routes,
vehicle overlap at the start, and grid rasterization of circles, polygons,
rotated boxes, and lane boundaries. Play or drag the sample slider to inspect
the vehicle body, safety margin, heading, steering, and curvature along the
returned path. Compare the detour scenes with and without smoothing. Playback
steps through path samples, not physical time or intermediate search states.
Grid-only cases do not run the planner; malformed-input tests have no spatial
scene. Re-run the executable after changing inputs or planner code.

The shared scenarios also include five harder static cases:

- Staggered parked cars requiring a route above one car and below the next.
- Construction clutter with a drum, angled barrier, and irregular debris.
- An offset approach through a 3.5 m gate between barriers.
- Ordered left/right lane boundaries forming an S bend, with a static drum.
- The same lane blocked by a barrier; planning must fail with an empty path.

These are synthetic scenes with illustrative dimensions. The lane edges define
the observed drivable corridor, so leaving that corridor is blocked. Successful
paths are checked for goal reach, bicycle-model consistency, steering limits,
and clearance of the full vehicle plus safety margin against occupied/unknown
cells and map edges. Intermediate arc poses are checked at intervals of at most
5 cm; this is a regression check, not a continuous collision proof. All five
cases appear in the visualizer and as separate CTest tests. To run only them:

```powershell
ctest --test-dir build/prove-planner -C Debug --output-on-failure -R "static_|lane_"
```

## Data contract

Include `prove/grid_builder.hpp` to build a map and `prove/planner.hpp` to plan
on it whenever a new consistent perception/localization snapshot is ready.
Both calls are synchronous; the caller owns scheduling and communication. The
example shows handling both successful and failed results.

### From perception geometry to a grid

1. Transform boundary points, obstacle centers and sizes, the current pose and
   the goal into the same frame at compatible times. Both boundary lines must be
   ordered in the same travel direction. Call `closeCorridor(left, right)` to
   connect their endpoints and form a closed polygon. It does not extrapolate
   the boundaries. Invalid or crossing geometry causes failure.
2. Specify the grid's lower-left origin, width and height in cells, and
   resolution in meters per cell. The grid must cover the observed corridor and
   the full vehicle footprint; outside the grid is blocked.
3. Call `buildOccupancyGrid` with the corridor and the obstacle shapes. A box
   takes its center, full length, full width and yaw; its length points along
   yaw. Circles take a center and radius. Closed obstacle polygons are supported
   too. If orientation is unavailable, use a conservative circle large enough
   to contain the entire obstacle (for a rectangle, radius at least
   `hypot(length, width) / 2`) rather than guessing its yaw. Sizes must include
   the uncertainty needed to cover the physical obstacle.
4. If either build step fails, clear the previous path and signal controls to
   stop. Otherwise call `plan(map.grid, start, goal, config)` and send the
   result, including failures, to controls. See `examples/planner_updates.cpp`.

The builder marks cells touched by a boundary or obstacle occupied, cells
outside the polygon unknown, and the remaining cells inside it free. **This
assumes the whole corridor interior was observed and that the supplied obstacle
list is complete for that observation.** Boundary lines and detected objects
alone cannot establish that occluded or unobserved space is free. Perception
must supply sufficient coverage information; mark any unobserved cells unknown
before planning, or reject the snapshot if coverage is not available. The
straight end caps between boundary endpoints are an assumption about the
observed region, not detected boundaries. Do not use this corridor helper if they
would include unobserved or non-drivable space.

The builder rasterizes physical obstacles without adding the vehicle footprint;
the planner checks the footprint and safety margin separately. Rebuild the grid
for each accepted snapshot so old obstacle locations are not silently retained.

### Planner inputs and outputs

- Coordinates and distances are in meters, angles in radians, with +x forward
  and +y left in a right-handed frame. `start` is the rear axle pose. The map,
  start, goal and output must all use the same frame. If using a vehicle-local
  frame, transform every new map and goal to the frame at the snapshot time.
- The map is a complete, axis-aligned grid; `origin_x, origin_y` is the lower
  left corner of cell (0, 0), rows increase in +y and columns increase in +x,
  and `cells[row * width + column]` is the occupancy value. Values below zero
  are unknown and blocked; values at or above `occupied_threshold` are blocked.
  Outside the grid is blocked. The grid must mark obstacles and all
  non-drivable regions/boundaries as occupied, and unobserved regions as
  unknown. Do not treat missing observations as free space. A rotated map
  origin must be transformed/resampled before calling this planner.
- Provide an explicit goal pose in the map frame. The planner ends within
  `goal_tolerance` of its position; heading is checked only when
  `require_goal_yaw` is enabled. It does not create a 30 m goal or extend a
  path through unobserved space. A caller may choose a reachable goal using
  route or boundary information, and should account for the vehicle footprint
  fitting inside the observed map at the goal.
- A successful result contains the rear-axle path sampled along bicycle-model
  arcs, including yaw, steering angle, and curvature. Each point's steering and
  curvature describe the incoming arc. The first point is the start pose. The
  path is geometry only: it contains no speed, timing, acceleration, or stop
  command. Controls must agree on and implement a safe speed profile and path
  tracking behavior. The result may stop short of the exact goal by the allowed
  tolerance; it does not append an unchecked segment to the goal. If the start
  is already within the requested tolerance, the result contains only the start
  pose; controls should treat that as goal reached, not a driving trajectory.
- On any unsuccessful result the path is empty. The integration must replace
  or invalidate the previous path on failure and direct controls to stop; it
  must not continue using an old path just because replanning failed.

For continuous operation, take consistent timestamped map and rear-axle pose
snapshots, call the planner on each update or at an agreed rate, and publish
every result to controls. A production adapter still needs to attach frame IDs,
timestamps, sequence IDs and validity periods, discard stale/out-of-order
inputs and results, and make controls stop if updates stop arriving. If planning
runs asynchronously, an older result must not overwrite a newer snapshot. These
policies depend on the teams' transport and clock conventions and are not
implemented by this library. No update rate is guaranteed.

## Vehicle configuration and limits

The `Config` defaults are example values, **not verified Mila measurements**.
Before vehicle use, set wheelbase, steering limit, front and rear extents from
the rear axle, half-width, and safety margin to measured values. The rectangular
footprint is checked against occupied and unknown cells and the map edge along
sampled motion primitives. Tune the grid resolution and margin for sensor,
localization and discretization error. Validate both the map and the resulting
path in simulation and in controlled tests before using it on the vehicle.

The planner penalizes steering magnitude and changes in steering to discourage
unnecessary turns and abrupt reversals. By default it also smooths the steering
sequence after search, re-integrating bicycle-model arcs and checking the full
footprint and goal tolerance for every candidate. If no improvement passes the
checks it keeps the original path. `smooth_path` disables this step and
`smoothing_applied` reports whether a candidate was accepted; `cost` remains
the original search cost. It limits curvature using the steering
limit and wheelbase, but it does not impose steering rate, acceleration, jerk,
vehicle dynamics, or obstacle motion. The steering-change penalty is not a
physical steering-rate guarantee: the smoothed path can still have abrupt
steering changes. A speed profile and actuator limits are needed to determine
whether the car can follow it. The initial steering cost assumes zero
steering. Search is discretized, forward-only, and has
an expansion limit: failure does not prove no feasible path exists. It does not
perform a swept-volume proof between samples, and it is not a complete vehicle
safety system.
