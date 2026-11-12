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

## Using the C++ sensor interface

The `ros2_sensor_interface` package provides the `SensorInput` class.
A localization program can use it to retrieve IMU samples,
magnetometer readings, and simulated ground-truth pose.

### Available functions

| Function | Returns | Intended use |
| --- | --- | --- |
| `drain_imu()` | A vector of queued IMU messages | Process every available IMU sample in arrival order. Calling it removes those samples from the queue. |
| `latest_mag()` | The latest magnetometer message, or `std::nullopt` | Read the latest magnetic-field measurement. |
| `getPosition()` | The latest fresh ground-truth `PoseStamped`, or `std::nullopt` | Compare a localization estimate with the simulated vehicle pose. |
| `status()` | Freshness information and counters | Check sensor availability, queue overflows, and timestamp events. |

`getPosition()` reports the simulated `/Mila/Mila_Root` pose in the
Isaac `world` frame. It is intended for evaluating an estimate; it is
not a simulated IMU, magnetometer, or GNSS measurement.

### Example

The ROS executor must keep processing callbacks for the interface to
receive new messages. This example demonstrates the API:

```cpp
#include <chrono>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "ros2_sensor_interface/sensor_input.hpp"

using namespace std::chrono_literals;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto sensors =
    std::make_shared<ros2_sensor_interface::SensorInput>();

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(sensors);

  const auto logger = rclcpp::get_logger("localization_example");
  auto last_report = std::chrono::steady_clock::now();

  while (rclcpp::ok()) {
    // Process incoming ROS messages.
    executor.spin_some();

    // Process every queued IMU sample.
    const auto imu_samples = sensors->drain_imu();

    for (const auto & imu : imu_samples) {
      const double accel_x = imu.linear_acceleration.x;
      const double accel_y = imu.linear_acceleration.y;
      const double accel_z = imu.linear_acceleration.z;

      const double gyro_x = imu.angular_velocity.x;
      const double gyro_y = imu.angular_velocity.y;
      const double gyro_z = imu.angular_velocity.z;

      // Use imu.header.stamp and imu.header.frame_id when processing
      // the measurements in your estimator.
      //
      // Replace these lines with your estimator update:
      (void)accel_x;
      (void)accel_y;
      (void)accel_z;
      (void)gyro_x;
      (void)gyro_y;
      (void)gyro_z;
    }

    const auto status = sensors->status();
    const auto mag = sensors->latest_mag();

    if (mag && status.mag_fresh) {
      // Magnetic-field values are in tesla.
      const double mag_x = mag->magnetic_field.x;
      const double mag_y = mag->magnetic_field.y;
      const double mag_z = mag->magnetic_field.z;

      // latest_mag() can return the same message on successive calls.
      // Use mag->header.stamp to recognize a new sample.
      (void)mag_x;
      (void)mag_y;
      (void)mag_z;
    }

    // Use ground truth to evaluate the estimate, not as a sensor input.
    const auto truth = sensors->getPosition();

    const auto now = std::chrono::steady_clock::now();

    if (now - last_report >= 1s) {
      if (truth) {
        RCLCPP_INFO(
          logger,
          "Ground truth [m]: x=%.3f y=%.3f z=%.3f, frame=%s",
          truth->pose.position.x,
          truth->pose.position.y,
          truth->pose.position.z,
          truth->header.frame_id.c_str());
      } else {
        RCLCPP_INFO(logger, "Ground truth unavailable or stale.");
      }

      RCLCPP_INFO(
        logger,
        "IMU fresh=%s, MAG fresh=%s, queue overflows=%lu",
        status.imu_fresh ? "yes" : "no",
        status.mag_fresh ? "yes" : "no",
        static_cast<unsigned long>(status.imu_queue_overflows));

      last_report = now;
    }

    // This sleep avoids busy-spinning; it does not guarantee a
    // particular sample-processing rate.
    std::this_thread::sleep_for(2ms);
  }

  rclcpp::shutdown();
  return 0;
}
```

### Integration notes

- Build your package with a dependency on `ros2_sensor_interface`.
  The `sensor_consumer_example` package shows how to link against it.
- `latest_mag()` may return a previously received message even when it
  is stale. Check `status().mag_fresh` and the message timestamp.
- The IMU queue has a finite capacity. Monitor
  `status().imu_queue_overflows`; an overflow means an older queued
  sample was discarded.
- `getPosition()` returns `std::nullopt` before a pose arrives and when
  the last pose exceeds the interface's freshness timeout.
- Ground-truth position describes the vehicle root. An estimator that
  reports the IMU position may have an offset due to sensor placement.
  Align coordinate frames, reference points, and timestamps before
  calculating an error.
- The ground-truth topic is produced by the Isaac simulation stage.
  It is not available from physical hardware without a separate
  reference-position system.
