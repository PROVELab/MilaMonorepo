# PROVE Autonomous Drive Stack

## Run the simulation demo

Prerequisites: Isaac Sim, ROS 2 Humble, `colcon`, and the localization
and perception dependencies described in their respective READMEs.

1. Open `simulation/isaac_stage/Map.usda` in Isaac Sim.
   On Windows, use a Windows-local copy of the simulation folder.
2. From `autonomous_drive_stack`, run:

   ```bash
   ./start_full_demo.sh
   ```

3. Press **Play** in Isaac Sim. Open the viewer URL printed in the
   terminal to see the camera feeds.

Press **Stop** in Isaac Sim to reset the drive. Press **Ctrl+C** in the
terminal to stop the localization and perception processes.

To change the drive test used by `Map.usda`, edit
[`simulation/localization/mila_vehicle_drive_test.py`](simulation/localization/mila_vehicle_drive_test.py).

ROS 2 must be configured to communicate between Isaac Sim and the
machine running the script.

For setup details and sensor-interface usage, see
[`simulation/localization/README.md`](simulation/localization/README.md)
and [`perception/README.md`](perception/README.md).
