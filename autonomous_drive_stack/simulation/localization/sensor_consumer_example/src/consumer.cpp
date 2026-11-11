#include <chrono>
#include <memory>
#include <optional>
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

  const auto logger = rclcpp::get_logger("consumer_example");
  auto last_report = std::chrono::steady_clock::now();

  // Keep the latest IMU reading between reports.
  std::optional<sensor_msgs::msg::Imu> latest_imu;

  RCLCPP_INFO(
    logger,
    "Sensor display started. Waiting for IMU and magnetometer data...");

  while (rclcpp::ok()) {
    // Process available ROS subscription callbacks.
    executor.spin_some();

    // A future estimator can process every sample in this batch.
    auto imu_samples = sensors->drain_imu();

    if (!imu_samples.empty()) {
      latest_imu = imu_samples.back();
    }

    const auto now = std::chrono::steady_clock::now();

    if (now - last_report >= 1s) {
      const auto status = sensors->status();
      const auto mag = sensors->latest_mag();

      RCLCPP_INFO(
        logger,
        "STATUS | IMU fresh: %s | MAG fresh: %s | "
        "IMU received: %lu | queue overflows: %lu",
        status.imu_fresh ? "yes" : "no",
        status.mag_fresh ? "yes" : "no",
        static_cast<unsigned long>(status.imu_received),
        static_cast<unsigned long>(status.imu_queue_overflows));

      if (latest_imu && status.imu_fresh) {
        RCLCPP_INFO(
          logger,
          "IMU acceleration [m/s^2] | X: % .3f | Y: % .3f | Z: % .3f "
          "| frame: %s",
          latest_imu->linear_acceleration.x,
          latest_imu->linear_acceleration.y,
          latest_imu->linear_acceleration.z,
          latest_imu->header.frame_id.c_str());

        RCLCPP_INFO(
          logger,
          "IMU angular velocity [rad/s] | X: % .4f | Y: % .4f | Z: % .4f "
          "| frame: %s",
          latest_imu->angular_velocity.x,
          latest_imu->angular_velocity.y,
          latest_imu->angular_velocity.z,
          latest_imu->header.frame_id.c_str());
      } else {
        RCLCPP_INFO(
          logger,
          "IMU readings unavailable or stale.");
      }

      if (mag && status.mag_fresh) {
        // sensor_msgs/MagneticField stores values in tesla.
        // Convert to microtesla for display.
        RCLCPP_INFO(
          logger,
          "MAG field [uT] | X: % .2f | Y: % .2f | Z: % .2f "
          "| frame: %s",
          mag->magnetic_field.x * 1e6,
          mag->magnetic_field.y * 1e6,
          mag->magnetic_field.z * 1e6,
          mag->header.frame_id.c_str());
      } else {
        RCLCPP_INFO(
          logger,
          "MAG readings unavailable or stale.");
      }

      last_report = now;
    }

    // Avoid busy-spinning. This display is not a 416 Hz guarantee.
    std::this_thread::sleep_for(2ms);
  }

  rclcpp::shutdown();
  return 0;
}