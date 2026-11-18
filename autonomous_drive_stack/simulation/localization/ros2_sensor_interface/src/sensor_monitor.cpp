#include <chrono>
#include <cinttypes>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "ros2_sensor_interface/sensor_input.hpp"

using namespace std::chrono_literals;
using ros2_sensor_interface::SensorInput;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto sensors = std::make_shared<SensorInput>();

  // ROS callbacks run in this executor thread. The main thread can then
  // access the samples through the class's mutex-protected functions.
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(sensors);

  std::thread ros_thread([&executor]() {
    executor.spin();
  });

  const auto logger = rclcpp::get_logger("sensor_monitor");

  while (rclcpp::ok()) {
    std::this_thread::sleep_for(1s);

    const auto imu_samples = sensors->drain_imu();
    const auto mag = sensors->latest_mag();
    const auto status = sensors->status();

    RCLCPP_INFO(
      logger,
      "IMU received=%" PRIu64 " | drained=%zu | fresh=%s | "
      "queue overflows=%" PRIu64 " | time resets=%" PRIu64,
      status.imu_received,
      imu_samples.size(),
      status.imu_fresh ? "yes" : "no",
      status.imu_queue_overflows,
      status.imu_time_resets);

    if (!imu_samples.empty()) {
      const auto & imu = imu_samples.back();

      RCLCPP_INFO(
        logger,
        "Latest drained IMU | stamp=%d.%09u | frame=%s | "
        "accel=[%.3f, %.3f, %.3f] m/s^2 | "
        "gyro=[%.4f, %.4f, %.4f] rad/s",
        imu.header.stamp.sec,
        imu.header.stamp.nanosec,
        imu.header.frame_id.c_str(),
        imu.linear_acceleration.x,
        imu.linear_acceleration.y,
        imu.linear_acceleration.z,
        imu.angular_velocity.x,
        imu.angular_velocity.y,
        imu.angular_velocity.z);
    }

    if (mag && status.mag_fresh) {
      RCLCPP_INFO(
        logger,
        "Latest MAG | stamp=%d.%09u | frame=%s | "
        "field=[%.2f, %.2f, %.2f] uT",
        mag->header.stamp.sec,
        mag->header.stamp.nanosec,
        mag->header.frame_id.c_str(),
        mag->magnetic_field.x * 1e6,
        mag->magnetic_field.y * 1e6,
        mag->magnetic_field.z * 1e6);
    } else {
      RCLCPP_WARN(logger, "No fresh magnetometer sample.");
    }

    if (!status.imu_fresh) {
      RCLCPP_WARN(logger, "No fresh IMU sample.");
    }
  }

  executor.cancel();

  if (ros_thread.joinable()) {
    ros_thread.join();
  }

  rclcpp::shutdown();
  return 0;
}