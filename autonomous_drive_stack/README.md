# PROVE Autonomous Drive Stack

## Run the simulation demo

Prerequisites: Isaac Sim, ROS 2 Humble, `colcon`, and the localization
and perception dependencies described in their respective READMEs.
## Isaac Sim stages

1. Both stages use the same camera, LiDAR, IMU, magnetometer, and
ground-truth pipelines:

- **`simulation/isaac_stage/Map.usda`** — runs the automatic drive
  test when you press Play. Edit
  `simulation/localization/mila_vehicle_drive_test.py` to change
  the test.
- **`simulation/isaac_stage/Map_manual.usda`** — lets you drive
  the vehicle manually. The automatic drive graph is disabled.

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
