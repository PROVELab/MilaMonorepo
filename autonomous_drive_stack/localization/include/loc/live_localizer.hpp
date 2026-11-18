#pragma once
// LiveLocalizer: the Localizer for live use on the car.
//
// On the car, readings arrive late and out of order: a GPS fix shows up
// ~50-100 ms after the moment it describes, CAN messages a few ms late. The
// Localizer needs them in time order (it drops anything older than data it has
// already used), so LiveLocalizer:
//   1. holds every reading for Config::fusion_delay_s,
//   2. then feeds the Localizer in time order (a late GPS fix lands where it belongs),
//   3. predicts the output forward from that delayed filter to the newest IMU
//      sample, so the car still gets a current position.
//
// Call everything from one task, in arrival order; `t` is always the time the
// reading was valid. No heap: both buffers are fixed-size arrays, which makes
// the object 22 KB (double) / 14 KB (float). Make it a static or global
// object, not a local variable on a FreeRTOS task's stack.

#include "loc/localizer.hpp"

namespace loc {

class LiveLocalizer {
 public:
  explicit LiveLocalizer(const Config& cfg = {});

  void set_datum(double lat_deg, double lon_deg, double alt_m) {
    filter_.set_datum(lat_deg, lon_deg, alt_m);
  }

  void on_imu(double t, const Vec3& accel_mps2, const Vec3& gyro_rps);
  void on_gps(const GpsFix& fix);
  void on_wheel_rpm(double t, Scalar motor_rpm);
  void on_steering(double t, Scalar road_wheel_angle_rad);
  void on_mag(double t, const Vec3& mag_raw);
  void on_expected_accel(double t, Scalar accel_mps2);

  Output output() const;  // predicted forward to the newest IMU sample
  std::uint32_t faults() const { return filter_.faults() | faults_; }
  void clear_faults();

  // Innovations of the corrections applied since the last clear_innovations(),
  // oldest first. If you never clear it, it just stops recording when full.
  std::span<const Innovation> innovations() const { return {innovations_.data(), size_t(n_innov_)}; }
  void clear_innovations() { n_innov_ = 0; }

  const Localizer& filter() const { return filter_; }  // the delayed filter

 private:
  struct Reading {  // a non-IMU reading waiting in the buffer
    enum class Kind : std::uint8_t { Gps, WheelRpm, Steering, Mag, ExpectedAccel };
    Kind kind{};
    double t = 0;
    GpsFix gps;               // Kind::Gps
    Vec3 vec = Vec3::Zero();  // Kind::Mag
    Scalar value = 0;         // the other kinds
  };
  static constexpr int kImuCapacity = 128;      // 0.28 s at 450 Hz, > fusion_delay_s
  static constexpr int kReadingCapacity = 64;   // ~2x what arrives in 0.15 s
  static constexpr int kInnovationCapacity = 32;

  void add(const Reading& r);
  void release(double cutoff);  // feed the filter everything valid at or before `cutoff`
  void apply(const Reading& r);
  void record(const std::optional<Innovation>& in);
  int oldest_reading() const;  // index into readings_, or -1 if empty

  Config cfg_;
  Localizer filter_;
  std::uint32_t faults_ = 0;  // faults seen here (IMU stopped), on top of the filter's

  bool have_imu_ = false;
  double t_newest_imu_ = 0;
  std::array<ImuSample, kImuCapacity> imu_{};  // not yet used by the filter, oldest first
  int n_imu_ = 0;
  std::array<Reading, kReadingCapacity> readings_{};  // waiting, in no particular order
  int n_readings_ = 0;
  std::array<Innovation, kInnovationCapacity> innovations_{};
  int n_innov_ = 0;
};

}  // namespace loc
