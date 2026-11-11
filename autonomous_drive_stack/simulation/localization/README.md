# MILA Localization Sensor Demo

This demo publishes simulated IMU and magnetometer data and provides a
reusable C++ sensor interface. By default, it also starts an example
consumer that displays labeled sensor readings.

## Prerequisites

- ROS 2 Humble and `colcon` installed in the Linux or WSL environment.
- The ROS 2 and Python dependencies required by the sensor packages.
- Isaac Sim with the ROS 2 bridge configured to communicate with the
  environment running the sensor nodes.
- The MILA stage and assets from this repository.

The demo has been tested with Isaac Sim 6.1 on Windows and ROS 2 Humble
in WSL. Linux workstation and Orin deployments still need testing on
their target hardware.

## Start the demo

From this directory, run:

```bash
./start_demo.sh
```

The launcher builds the C++ packages and starts the sensor pipeline and
example consumer. On Windows with WSL, it also copies the stage files to
a Windows-local folder and prints the path. On Linux, it prints the
repository stage path.

Open Isaac Sim, open the printed stage, and press **Play**. The vehicle
should perform its test drive. The example consumer displays:

- Acceleration along X, Y, and Z in m/s².
- Angular velocity about X, Y, and Z in rad/s.
- Magnetic field along X, Y, and Z in microtesla.
- Sensor freshness, IMU message count, and queue overflows.

Press **Stop** in Isaac Sim to stop and reset the drive test. Press
**Ctrl+C** in the launcher terminal to stop the sensor processes.

## Launcher options

### Run without the example consumer

Use this when your own program will consume the sensor data:

```bash
./start_demo.sh --no-consumer
```

The sensor publishers run, but the example display does not.

### Start the sensor pipeline without preparing a stage

Use this when Isaac Sim is running on another machine:

```bash
./start_demo.sh --sensors-only
```

This starts the sensor pipeline and example consumer without preparing
or printing a local stage. ROS 2 communication between the machines must
be configured and tested.

To also omit the example consumer:

```bash
./start_demo.sh --sensors-only --no-consumer
```

### Show help

```bash
./start_demo.sh --help
```

## Important files

- `start_demo.sh`: shared demo entry point.
- `start_sensor_demo.sh`: builds the C++ packages and starts the sensor
  processes.
- `mila_vehicle_drive_graph.py`: drive-test code loaded by the stage.
- `ros2_sensor_interface/`: reusable C++ sensor interface.
- `sensor_consumer_example/`: example program that displays readings.

When adding a sensor process that the demo needs to run, update the
shared sensor launcher as part of the same change.
