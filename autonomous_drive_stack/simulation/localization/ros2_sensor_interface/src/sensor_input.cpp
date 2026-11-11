#include "ros2_sensor_interface/sensor_input.hpp"

#include <stdexcept>

namespace ros2_sensor_interface
{

SensorInput::SensorInput(
  std::size_t imu_queue_capacity,
  std::chrono::milliseconds stale_timeout)
: Node("sensor_input_cpp"),
  imu_queue_capacity_(imu_queue_capacity),
  stale_timeout_(stale_timeout)
{
  if (imu_queue_capacity_ == 0) {
    throw std::invalid_argument("IMU queue capacity must be positive");
  }

  if (stale_timeout_.count() <= 0) {
    throw std::invalid_argument("Stale timeout must be positive");
  }

  // Suitable for sensor streams. Best-effort delivery is not guaranteed;
  // later tests must measure whether messages are dropped.
  const auto qos = rclcpp::SensorDataQoS();

  imu_subscription_ = create_subscription<Imu>(
    "/imu_noisy",
    qos,
    [this](Imu::ConstSharedPtr msg) {
      imu_callback(msg);
    });

  mag_subscription_ = create_subscription<MagneticField>(
    "/mag",
    qos,
    [this](MagneticField::ConstSharedPtr msg) {
      mag_callback(msg);
    });

  RCLCPP_INFO(
    get_logger(),
    "Sensor interface listening on /imu_noisy and /mag");
}

std::int64_t SensorInput::timestamp_ns(
  std::int32_t seconds,
  std::uint32_t nanoseconds)
{
  return static_cast<std::int64_t>(seconds) * 1'000'000'000LL +
         static_cast<std::int64_t>(nanoseconds);
}

void SensorInput::imu_callback(Imu::ConstSharedPtr msg)
{
  const auto stamp = timestamp_ns(
    msg->header.stamp.sec,
    msg->header.stamp.nanosec);

  std::lock_guard<std::mutex> lock(mutex_);

  if (last_imu_stamp_ns_) {
    if (stamp == *last_imu_stamp_ns_) {
      ++imu_duplicate_timestamps_;
      return;
    }

    if (stamp < *last_imu_stamp_ns_) {
      // A simulator restart is one possibility, but an out-of-order message
      // can also cause this. Inform the consumer through the reset counter.
      ++imu_time_resets_;
      imu_queue_.clear();

      RCLCPP_WARN(
        get_logger(),
        "IMU time moved backward; cleared the old IMU queue.");
    }
  }

  last_imu_stamp_ns_ = stamp;
  last_imu_receive_time_ = SteadyClock::now();
  ++imu_received_;

  if (imu_queue_.size() >= imu_queue_capacity_) {
    // Keep the newest sample, but make the dropped sample visible in status.
    imu_queue_.pop_front();
    ++imu_queue_overflows_;
  }

  imu_queue_.push_back(*msg);
}

void SensorInput::mag_callback(MagneticField::ConstSharedPtr msg)
{
  const auto stamp = timestamp_ns(
    msg->header.stamp.sec,
    msg->header.stamp.nanosec);

  std::lock_guard<std::mutex> lock(mutex_);

  if (last_mag_stamp_ns_) {
    if (stamp == *last_mag_stamp_ns_) {
      ++mag_duplicate_timestamps_;
      return;
    }

    if (stamp < *last_mag_stamp_ns_) {
      ++mag_time_resets_;

      RCLCPP_WARN(
        get_logger(),
        "Magnetometer time moved backward; replacing its latest sample.");
    }
  }

  last_mag_stamp_ns_ = stamp;
  last_mag_receive_time_ = SteadyClock::now();
  latest_mag_ = *msg;
  ++mag_received_;
}

std::vector<SensorInput::Imu> SensorInput::drain_imu()
{
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<Imu> result;
  result.reserve(imu_queue_.size());

  while (!imu_queue_.empty()) {
    result.push_back(std::move(imu_queue_.front()));
    imu_queue_.pop_front();
  }

  return result;
}

std::optional<SensorInput::MagneticField> SensorInput::latest_mag() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return latest_mag_;
}

SensorInput::Status SensorInput::status() const
{
  std::lock_guard<std::mutex> lock(mutex_);

  const auto now = SteadyClock::now();

  const bool imu_fresh =
    last_imu_receive_time_.has_value() &&
    (now - *last_imu_receive_time_ <= stale_timeout_);

  const bool mag_fresh =
    last_mag_receive_time_.has_value() &&
    (now - *last_mag_receive_time_ <= stale_timeout_);

  return Status{
    last_imu_receive_time_.has_value(),
    last_mag_receive_time_.has_value(),
    imu_fresh,
    mag_fresh,
    imu_queue_.size(),
    imu_received_,
    mag_received_,
    imu_queue_overflows_,
    imu_duplicate_timestamps_,
    imu_time_resets_,
    mag_duplicate_timestamps_,
    mag_time_resets_
  };
}

}  // namespace ros2_sensor_interface