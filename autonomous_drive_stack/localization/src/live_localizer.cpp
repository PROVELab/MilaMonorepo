#include "loc/live_localizer.hpp"

#include <algorithm>

namespace loc {

LiveLocalizer::LiveLocalizer(const Config& cfg) : cfg_(cfg), filter_(cfg) {}

void LiveLocalizer::on_imu(double t, const Vec3& accel_mps2, const Vec3& gyro_rps) {
  if (have_imu_ && t <= t_newest_imu_) return;  // duplicate / out-of-order sample
  have_imu_ = true;
  t_newest_imu_ = t;
  if (n_imu_ == kImuCapacity) release(imu_[0].t);  // full (delay too long for the buffer)
  imu_[n_imu_++] = {t, accel_mps2, gyro_rps};
  release(t - cfg_.fusion_delay_s);
}

void LiveLocalizer::on_gps(const GpsFix& fix) {
  add({.kind = Reading::Kind::Gps, .t = fix.t, .gps = fix});
}

void LiveLocalizer::on_wheel_rpm(double t, Scalar motor_rpm) {
  add({.kind = Reading::Kind::WheelRpm, .t = t, .value = motor_rpm});
}

void LiveLocalizer::on_steering(double t, Scalar road_wheel_angle_rad) {
  add({.kind = Reading::Kind::Steering, .t = t, .value = road_wheel_angle_rad});
}

void LiveLocalizer::on_mag(double t, const Vec3& mag_raw) {
  add({.kind = Reading::Kind::Mag, .t = t, .vec = mag_raw});
}

void LiveLocalizer::on_expected_accel(double t, Scalar accel_mps2) {
  add({.kind = Reading::Kind::ExpectedAccel, .t = t, .value = accel_mps2});
}

Output LiveLocalizer::output() const {
  Output o = filter_.predict_output({imu_.data(), size_t(n_imu_)});
  o.faults |= faults_;
  o.ready = o.ready && faults_ == 0;
  return o;
}

void LiveLocalizer::clear_faults() {
  faults_ = 0;
  filter_.clear_faults();
}

void LiveLocalizer::add(const Reading& r) {
  // Readings are never much newer than the IMU unless the IMU has stopped.
  if (have_imu_ && r.t - t_newest_imu_ > cfg_.max_imu_gap_s) faults_ |= kFaultImuGap;
  if (n_readings_ == kReadingCapacity) release(readings_[oldest_reading()].t);  // make room
  readings_[n_readings_++] = r;
}

void LiveLocalizer::release(double cutoff) {
  // Merge the two buffers by time. At equal times the IMU goes first, so the
  // correction sees the latest state (same rule as tools/replay.cpp).
  int used = 0;  // IMU samples handed to the filter
  while (true) {
    const int r = oldest_reading();
    const bool imu_due = used < n_imu_ && imu_[used].t <= cutoff;
    const bool reading_due = r >= 0 && readings_[r].t <= cutoff;
    if (imu_due && (!reading_due || imu_[used].t <= readings_[r].t)) {
      filter_.on_imu(imu_[used].t, imu_[used].accel, imu_[used].gyro);
      ++used;
    } else if (reading_due) {
      apply(readings_[r]);
      readings_[r] = readings_[--n_readings_];  // remove it (order doesn't matter)
    } else {
      break;
    }
  }
  // Keep the IMU samples the filter hasn't used, oldest first.
  std::copy(imu_.begin() + used, imu_.begin() + n_imu_, imu_.begin());
  n_imu_ -= used;
}

void LiveLocalizer::apply(const Reading& r) {
  switch (r.kind) {
    case Reading::Kind::Gps: {
      const GpsInnovations g = filter_.on_gps(r.gps);
      record(g.pos);
      record(g.vel);
      break;
    }
    case Reading::Kind::WheelRpm:
      record(filter_.on_wheel_rpm(r.t, r.value));
      break;
    case Reading::Kind::Steering:
      record(filter_.on_steering(r.t, r.value));
      break;
    case Reading::Kind::Mag:
      record(filter_.on_mag(r.t, r.vec));
      break;
    case Reading::Kind::ExpectedAccel:
      record(filter_.on_expected_accel(r.t, r.value));
      break;
  }
}

void LiveLocalizer::record(const std::optional<Innovation>& in) {
  if (in && n_innov_ < kInnovationCapacity) innovations_[n_innov_++] = *in;
}

int LiveLocalizer::oldest_reading() const {
  int best = -1;
  for (int i = 0; i < n_readings_; ++i) {
    if (best < 0 || readings_[i].t < readings_[best].t) best = i;
  }
  return best;
}

}  // namespace loc
