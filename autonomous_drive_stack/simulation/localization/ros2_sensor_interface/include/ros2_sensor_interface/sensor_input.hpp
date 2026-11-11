#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/magnetic_field.hpp"

namespace ros2_sensor_interface
{

class SensorInput : public rclcpp::Node
{
public:
  using Imu = sensor_msgs::msg::Imu;
  using MagneticField = sensor_msgs::msg::MagneticField;

  struct Status
  {
    bool has_imu;
    bool has_mag;

    bool imu_fresh;
    bool mag_fresh;

    std::size_t imu_queue_size;

    std::uint64_t imu_received;
    std::uint64_t mag_received;

    std::uint64_t imu_queue_overflows;
    std::uint64_t imu_duplicate_timestamps;
    std::uint64_t imu_time_resets;
    std::uint64_t mag_duplicate_timestamps;
    std::uint64_t mag_time_resets;
  };

  explicit SensorInput(
    std::size_t imu_queue_capacity = 1000,
    std::chrono::milliseconds stale_timeout =
      std::chrono::milliseconds(2000));

  // Removes and returns all currently queued IMU samples, in arrival order.
  std::vector<Imu> drain_imu();

  // Returns the latest magnetometer sample, or std::nullopt if none exists.
  std::optional<MagneticField> latest_mag() const;

  // Gives a snapshot of data freshness and counters.
  Status status() const;

private:
  using SteadyClock = std::chrono::steady_clock;

  static std::int64_t timestamp_ns(
    std::int32_t seconds,
    std::uint32_t nanoseconds);

  void imu_callback(Imu::ConstSharedPtr msg);
  void mag_callback(MagneticField::ConstSharedPtr msg);

  const std::size_t imu_queue_capacity_;
  const std::chrono::milliseconds stale_timeout_;

  mutable std::mutex mutex_;

  std::deque<Imu> imu_queue_;
  std::optional<MagneticField> latest_mag_;

  std::optional<std::int64_t> last_imu_stamp_ns_;
  std::optional<std::int64_t> last_mag_stamp_ns_;

  std::optional<SteadyClock::time_point> last_imu_receive_time_;
  std::optional<SteadyClock::time_point> last_mag_receive_time_;

  std::uint64_t imu_received_ = 0;
  std::uint64_t mag_received_ = 0;

  std::uint64_t imu_queue_overflows_ = 0;
  std::uint64_t imu_duplicate_timestamps_ = 0;
  std::uint64_t imu_time_resets_ = 0;
  std::uint64_t mag_duplicate_timestamps_ = 0;
  std::uint64_t mag_time_resets_ = 0;

  rclcpp::Subscription<Imu>::SharedPtr imu_subscription_;
  rclcpp::Subscription<MagneticField>::SharedPtr mag_subscription_;
};

}  // namespace ros2_sensor_interface