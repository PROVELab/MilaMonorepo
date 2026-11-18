# Path Planning Visualizer

An interactive C++17 and raylib simulator for visualizing Hybrid A* path planning on an occupancy grid. A simulated vehicle follows a curved lane, avoids static obstacles, and replans as it moves. You can choose a destination with the mouse or let it follow an automatic goal ahead on the lane.

## Files

- `hybrid_astar_ogm_sim.cpp`: hybrid motion adapter, occupancy grid, simulated course, path playback, and visualization.
- `AStarProgram.h` / `AStarProgram.cpp`: shared A* search with adapter-defined transition costs and heuristics, continuous pose records, goal predicates, and expansion limits.
- `test_window.cpp`: minimal raylib window for checking the graphics setup.

The simulator and graphics check each have their own `main()`. Link the simulator with `AStarProgram.cpp`; build the graphics check separately.

## Windows setup with MSYS2 UCRT64

Run the commands below in **MSYS2 UCRT64**. These commands use its Bash shell; use that terminal instead of regular Command Prompt, PowerShell, or MSYS2 MSYS. VS Code is optional for editing the files.

### 1. Install MSYS2

Download and install [MSYS2](https://www.msys2.org/), keeping the default installation location: `C:\msys64`.

From the Windows Start menu, open **MSYS2 UCRT64**.

### 2. Update MSYS2

Run:

```bash
pacman -Syu
```

If the update tells you to close the terminal, close it. Reopen **MSYS2 UCRT64**, then run the update again:

```bash
pacman -Syu
```

### 3. Install C++, raylib, and pkg-config

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-raylib mingw-w64-ucrt-x86_64-pkgconf
```

When asked to proceed, type `Y`. Check that the compiler and raylib are available:

```bash
g++ --version
pkg-config --modversion raylib
```

The second command should print a version number.

### 4. Enter the project folder

If you are working in this repository, enter its `pathCreation` folder. For the current checkout:

```bash
cd /c/Users/nlvat/gitRepos/MilaMonorepo/autonomous_drive_stack/pathCreation
```

Adjust that path if you cloned the repository elsewhere.

Alternatively, if you downloaded the source files to your Windows Downloads folder, copy them into a new project folder. Replace `YOUR_WINDOWS_USERNAME` with your Windows username. This example assumes the downloaded simulator is named `hybrid_astar_ogm_sim(1).cpp`; adjust the source filename if needed.

```bash
mkdir -p ~/autonomy-sim
cp "/c/Users/YOUR_WINDOWS_USERNAME/Downloads/hybrid_astar_ogm_sim(1).cpp" ~/autonomy-sim/hybrid_astar_ogm_sim.cpp
cp "/c/Users/YOUR_WINDOWS_USERNAME/Downloads/AStarProgram.cpp" ~/autonomy-sim/AStarProgram.cpp
cp "/c/Users/YOUR_WINDOWS_USERNAME/Downloads/AStarProgram.h" ~/autonomy-sim/AStarProgram.h
cp "/c/Users/YOUR_WINDOWS_USERNAME/Downloads/test_window.cpp" ~/autonomy-sim/test_window.cpp
cd ~/autonomy-sim
```

Check the files:

```bash
ls
```

You should see all four source/header files:

```text
hybrid_astar_ogm_sim.cpp
AStarProgram.cpp
AStarProgram.h
test_window.cpp
```

The simulator requires both shared A* files alongside its source.

### 5. Test that raylib can open a window

Compile and run the graphics check:

```bash
g++ -std=c++17 test_window.cpp -o test_window.exe $(pkg-config --cflags --libs raylib)
./test_window.exe
```

A small raylib window should appear. Close it before continuing.

### 6. Compile and run the Hybrid A* simulator

```bash
g++ -std=c++17 -O2 hybrid_astar_ogm_sim.cpp AStarProgram.cpp -o hybrid_astar_ogm_sim.exe $(pkg-config --cflags --libs raylib)
./hybrid_astar_ogm_sim.exe
```

The simulator opens a 1200 x 800 window and starts following its planned path automatically. Left-click to set a goal, press **A** to restore the automatic goal, right-click to add an obstacle, and press **R** to reset the vehicle. The current simulator moves automatically; arrow keys do not control it. See the full controls below.

### After changing the code

Close the simulator, return to your project folder in **MSYS2 UCRT64**, and repeat:

```bash
g++ -std=c++17 -O2 hybrid_astar_ogm_sim.cpp AStarProgram.cpp -o hybrid_astar_ogm_sim.exe $(pkg-config --cflags --libs raylib)
./hybrid_astar_ogm_sim.exe
```

For the downloaded-file setup, enter `cd ~/autonomy-sim` first. For a repository checkout, use its `pathCreation` path from step 4. No additional assets or command-line arguments are required. Build and runtime verification of the shared A* integration remains pending.

## Controls

| Input | Action |
| --- | --- |
| Left mouse button | Set an exact destination at the clicked world position and trigger replanning. |
| A | Return to the automatic goal ahead on the lane. |
| Right mouse button | Add a fixed obstacle at the clicked position below the status bar and replan immediately. Works while paused. |
| O | Open obstacle coordinate entry. Type world coordinates as `x, y` or `x y`, then press Enter to add or Escape to cancel. Movement pauses while editing. |
| Mouse wheel over coordinate panel | Scroll the obstacle position list. |
| Space | Pause or resume vehicle movement. Planning continues while paused. |
| B | Enable or disable lane/track boundaries for both display and collision checking. Disabled by default; obstacles remain active. |
| R | Return the vehicle to its starting pose and trigger replanning. The current goal mode and pause state are retained. |
| Escape or window close button | Exit. |

Choose a clicked destination inside the local planning region and, when boundaries are enabled, inside the drivable corridor. Clicked goals are not automatically moved into free space. A blocked or unreachable goal produces a planner status message and stops the vehicle.

## Display

| Color | Meaning |
| --- | --- |
| Green lines | Lane boundaries. |
| Red circles | Static obstacles. |
| Blue line | Current planned path. |
| Purple crosshair | Current destination. |
| Black triangle | Vehicle position and heading. |

The view follows the vehicle's position without rotating with its heading. The status bar shows planning success or failure, the configured minimum turning radius, the number of expanded search states, and the pause state. The occupancy grid and inflated obstacle regions are used internally but are not drawn.

The right panel shows the car and goal world coordinates in meters, the car heading in degrees, and numbered obstacle coordinates. Obstacle numbers match their labels in the scene. Scene clicks to place obstacles or goals are accepted below the status bar and to the left of this panel.

## How planning works

1. Generate a fixed curved test course with two static obstacles in `makeTrack()`.
2. Transform visible lane edges and obstacles into vehicle coordinates: positive x is forward and positive y is left.
3. Build a local occupancy grid. When boundaries are enabled, cells outside the lane or too close to its boundaries are blocked. Automatic goals still follow the lane centerline when boundaries are disabled. Obstacles are enlarged by the vehicle half-width and safety margin.
4. Select a goal up to 30 m ahead along the computed centerline, or use the clicked destination. Automatic goals shorten near the course end and retreat along the centerline if needed to reach free space.
5. Use the shared search in `AStarProgram.cpp` with adapter-defined distance plus steering costs and a heuristic measuring distance to the goal tolerance region. Each search state consists of a grid cell and a heading bin. Expand forward bicycle-model arcs using left, straight, and right steering choices, checking samples along each arc against the grid.
6. Convert a successful path to world coordinates and play back its arcs at 4 m/s. Replace the path on each replan; stop if planning fails or the path endpoint is reached.

The replan interval is 0.08 seconds, targeting 12.5 Hz when computation allows. Rendering targets 60 FPS. Both planning and rendering run on the same thread.

## Configuration

Change these values in `hybrid_astar_ogm_sim.cpp`, then rebuild:

| Location | Parameter | Default |
| --- | --- | --- |
| `HybridConfig` | Wheelbase | 1.6 m |
| `HybridConfig` | Maximum steering angle | 0.42 radians |
| `HybridConfig` | Motion primitive length | 0.75 m |
| `HybridConfig` | Heading bins | 24 |
| `HybridConfig` | Goal position tolerance | 0.8 m |
| `HybridConfig` | Search expansion limit | 50,000 |
| `buildOGM()` | Grid resolution | 0.25 m |
| `buildOGM()` | Local grid bounds | x: [-2, 35) m; y: [-12, 12) m |
| `main()` | Vehicle half-width | 0.75 m |
| `main()` | Safety margin | 0.35 m |
| `main()` | Obstacle radius | 0.25 m |
| `main()` | Automatic goal lookahead | 30 m |
| `main()` | Playback speed | 4 m/s |
| `makeTrack()` | Lane half-width | 4.8 m |

The minimum turning radius is `wheelbase / tan(maximum steering angle)`, approximately 3.58 m with the defaults.

## Scope and limitations

The current waypoint setup is limited: waypoints stay at the same local position instead of moving as the vehicle progresses. Next, we will add waypoint updates as the vehicle moves. The current 30 m lookahead is used to showcase the path planning; the intended operating lookahead will likely be approximately 1 m.

This is a standalone simulation using generated lane boundaries and obstacles. It has no live perception, vehicle hardware, ROS integration, or input dataset loading.

The planner uses forward motion only and checks goal position without requiring a final heading. Collision checking uses an inflated grid around the vehicle reference point, rather than an oriented rectangular vehicle footprint. Path following plays back planned arcs; it does not model steering actuator response, acceleration, or tracking error.

To experiment with other courses, edit `makeTrack()`. The perception-style inputs are ordered left and right lane-edge points and static obstacle positions.

`makeTrack(const std::vector<Point>& obstacles)` accepts the initial obstacle positions; edit `initial_obstacles` in `main()` to customize them. Each `Point` is an `{x, y}` world position in meters. Pass an empty vector for no initial obstacles. Use `addObstacles(track, {{10.0, 1.0}, {15.0, -1.0}})` to append more obstacles to an existing track. During simulation, right-click to place obstacles; they stay fixed in the world as the vehicle moves and last until the simulator closes. Pressing R retains all obstacles.

## Troubleshooting

- **`g++` or `pkg-config` is not found:** open **MSYS2 UCRT64** and install the packages from step 3. These instructions require that terminal.
- **`raylib.h` is missing or the linker cannot find raylib:** ensure raylib is installed for the same UCRT64 environment as the compiler. Avoid mixing UCRT64 and MINGW64 libraries.
- **A DLL is missing at launch:** run the executable from **MSYS2 UCRT64** so its compiler and raylib runtime DLLs are available.
- **Multiple definitions of `main`:** build `test_window.cpp` separately. Build the simulator with `hybrid_astar_ogm_sim.cpp AStarProgram.cpp`, as shown above.
- **Undefined references to `path_planning::findPath`:** include `AStarProgram.cpp` in the simulator compile command.
- **The vehicle stops:** read the planner status. Press A to restore automatic goals, or press R to return to the start. R retains a clicked goal; press A as well if you want automatic mode.
- **Rebuilding reports permission denied for the executable:** close the simulator window and rebuild.
