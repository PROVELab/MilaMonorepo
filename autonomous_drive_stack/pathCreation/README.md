# Path Planning Visualizer

An interactive C++17 and raylib simulator for visualizing Hybrid A* path planning on an occupancy grid. A simulated vehicle follows a curved lane, avoids static obstacles, and replans as it moves. You can choose a destination with the mouse or let it follow an automatic goal ahead on the lane.

## Files

- `hybrid_astar_ogm_sim.cpp`: complete planner, occupancy grid, simulated course, path playback, and visualization.
- `test_window.cpp`: minimal raylib window for checking the graphics setup.

Each source file has its own `main()`. Build them separately.

## Requirements

- A C++17 compiler.
- raylib headers and libraries.
- On the current Windows setup: MSYS2 UCRT64, installed at `C:\msys64`.
- Optional: VS Code with the Microsoft C/C++ extension for editing and IntelliSense.

VS Code is the editor and can invoke the compiler. MSYS2 supplies the `g++` compiler and raylib dependencies; you can build directly in VS Code's integrated terminal without opening an MSYS2 terminal.

## Build and run on Windows with VS Code

1. Open this folder in VS Code: `C:\Users\nlvat\gitRepos\pathPlanningVisualizer`.
2. Open **Terminal > New Terminal** and use PowerShell.
3. Run:

```powershell
# Make the compiler and runtime DLLs available in this terminal session.
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

# Compile the simulator.
g++ -std=c++17 -O2 hybrid_astar_ogm_sim.cpp -o hybrid_astar_ogm_sim.exe -lraylib -lopengl32 -lgdi32 -lwinmm

# Launch the simulator.
.\hybrid_astar_ogm_sim.exe
```

The simulator opens in a separate 1200 x 800 window and starts moving automatically. Recompile after changing the source. Close the running simulator before rebuilding its executable.

The PATH change lasts only for the current terminal session. Run it again in a new terminal, or add `C:\msys64\ucrt64\bin` to your Windows user PATH.

To build and run the smaller graphics check instead:

```powershell
g++ -std=c++17 -O2 test_window.cpp -o test_window.exe -lraylib -lopengl32 -lgdi32 -lwinmm
.\test_window.exe
```

The build command for the main simulator has been compiled and linked successfully with the installed UCRT64 toolchain. No additional assets or command-line arguments are required.

## Controls

| Input | Action |
| --- | --- |
| Left mouse button | Set an exact destination at the clicked world position and trigger replanning. |
| Right mouse button | Return to the automatic goal ahead on the lane. |
| Space | Pause or resume vehicle movement. Planning continues while paused. |
| R | Return the vehicle to its starting pose and trigger replanning. The current goal mode and pause state are retained. |
| Escape or window close button | Exit. |

Choose a clicked destination inside the drivable corridor and the local planning region. Clicked goals are not automatically moved into free space. A blocked or unreachable goal produces a planner status message and stops the vehicle.

## Display

| Color | Meaning |
| --- | --- |
| Green lines | Lane boundaries. |
| Red circles | Static obstacles. |
| Blue line | Current planned path. |
| Purple crosshair | Current destination. |
| Black triangle | Vehicle position and heading. |

The view follows the vehicle's position without rotating with its heading. The status bar shows planning success or failure, the configured minimum turning radius, the number of expanded search states, and the pause state. The occupancy grid and inflated obstacle regions are used internally but are not drawn.

## How planning works

1. Generate a fixed curved test course with two static obstacles in `makeTrack()`.
2. Transform visible lane edges and obstacles into vehicle coordinates: positive x is forward and positive y is left.
3. Build a local occupancy grid. Cells outside the lane or too close to its boundaries are blocked. Obstacles are enlarged by the vehicle half-width and safety margin.
4. Select a goal up to 30 m ahead along the computed centerline, or use the clicked destination. Automatic goals shorten near the course end and retreat along the centerline if needed to reach free space.
5. Search states consisting of a grid cell and a heading bin. Expand forward bicycle-model arcs using left, straight, and right steering choices, checking samples along each arc against the grid.
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

## Troubleshooting

- **`g++` is not recognized:** run the PATH command above and verify that `C:\msys64\ucrt64\bin\g++.exe` exists.
- **`raylib.h` is missing or the linker cannot find raylib:** ensure raylib is installed for the same UCRT64 environment as the compiler. Avoid mixing UCRT64 and MINGW64 libraries.
- **A DLL is missing at launch:** run from the terminal with the UCRT64 PATH configured. Keep that path available even when launching an already compiled executable.
- **Multiple definitions of `main`:** compile only one of the two source files per executable.
- **The vehicle stops:** read the planner status. Right-click to restore automatic goals, or press R to return to the start. R retains a clicked goal; right-click as well if you want automatic mode.
- **Rebuilding reports permission denied for the executable:** close the simulator window and rebuild.
