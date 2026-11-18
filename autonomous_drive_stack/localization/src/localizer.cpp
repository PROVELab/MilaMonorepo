#include "loc/localizer.hpp"

#include <GeographicLib/NormalGravity.hpp>
#include <Eigen/Geometry>
#include <cmath>

namespace loc {
namespace {

constexpr Scalar kPi = Scalar(3.14159265358979323846);
constexpr double kRecentS = 0.5;  // s; at start-up, a reading this recent still describes "now"

using Mat3xN = Eigen::Matrix<Scalar, 3, kStates>;
using Mat1xN = Eigen::Matrix<Scalar, 1, kStates>;
using Vec1 = Eigen::Matrix<Scalar, 1, 1>;
using Mat1 = Eigen::Matrix<Scalar, 1, 1>;

Mat3 rot_z(Scalar yaw) { return Eigen::AngleAxis<Scalar>(yaw, Vec3::UnitZ()).toRotationMatrix(); }

SO3 to_so3(const Mat3& R) { return SO3(Eigen::Quaternion<Scalar>(R).normalized()); }

Mat3 diag3(Scalar a, Scalar b, Scalar c) { return Vec3(a * a, b * b, c * c).asDiagonal(); }

}  // namespace

Localizer::Localizer(const Config& cfg) : cfg_(cfg) { nis_ratio_.fill(1); }

void Localizer::set_datum(double lat_deg, double lon_deg, double alt_m) {
  enu_.Reset(lat_deg, lon_deg, alt_m);
  double gy = 0, gz = 0;  // WGS84 normal gravity at the datum (north, up components)
  GeographicLib::NormalGravity::WGS84().Gravity(lat_deg, alt_m, gy, gz);
  gravity_ = Vec3(0, Scalar(gy), Scalar(gz));
  have_datum_ = true;
}

// ---------------------------------------------------------------- IMU / prediction

void Localizer::on_imu(double t, const Vec3& accel_mps2, const Vec3& gyro_rps) {
  const Vec3 f = cfg_.R_body_imu * accel_mps2;
  const Vec3 w = cfg_.R_body_imu * gyro_rps;
  if (have_imu_) {
    const double dt = t - t_imu_;
    if (dt <= 0) return;  // duplicate / out-of-order sample
    if (dt > cfg_.max_imu_gap_s && status_ == Status::Running) faults_ |= kFaultImuGap;
  }

  if (status_ == Status::Aligning) align(t, f, w);
  // The sample describes the interval that just ended, so it propagates [t_filter_, t].
  if (status_ == Status::Running) {
    propagate(t, f, w);
    check_health();
  }
  have_imu_ = true;
  t_imu_ = t;
  f_held_ = f;
  w_held_ = w;
}

void Localizer::propagate(double t, const Vec3& f, const Vec3& w) {
  const double dt = t - t_filter_;
  if (dt <= 0) return;
  eskf_.predict(f, w, Scalar(dt), cfg_);
  t_filter_ = t;
}

void Localizer::align(double t, const Vec3& f, const Vec3& w) {
  const bool still = w.norm() < cfg_.align_max_gyro &&
                     std::abs(f.norm() - Scalar(9.80665)) < cfg_.align_max_accel_dev;
  if (!still) {
    n_align_ = 0;
    return;
  }
  if (n_align_ == 0) {
    t_align0_ = t;
    sum_f_.setZero();
    sum_w_.setZero();
  }
  sum_f_ += f;
  sum_w_ += w;
  ++n_align_;
  if (t - t_align0_ < cfg_.align_time_s) return;

  // At rest the accelerometer reads +g along body "up": that gives roll and pitch.
  const Vec3 fm = sum_f_ / Scalar(n_align_);
  const Scalar roll = std::atan2(fm.y(), fm.z());
  const Scalar pitch = std::atan2(-fm.x(), std::hypot(fm.y(), fm.z()));
  R_level_ = (Eigen::AngleAxis<Scalar>(pitch, Vec3::UnitY()) *
              Eigen::AngleAxis<Scalar>(roll, Vec3::UnitX()))
                 .toRotationMatrix();
  bg0_ = sum_w_ / Scalar(n_align_);
  status_ = Status::WaitingForHeading;
}

// ---------------------------------------------------------------- initialisation

void Localizer::try_init_from_gps(const GpsFix& fix, const Vec3& p_ant) {
  const Scalar pos_sigma = std::max(fix.h_acc, cfg_.gps_pos_sigma_floor);

#if LOC_USE_MAG
  // Heading from the magnetometer, if we have a recent reading (best at standstill).
  if (have_mag_ && std::abs(fix.t - t_mag_) < kRecentS) {
    const Vec3 m_level = R_level_ * mag_body_;
    const Scalar decl = cfg_.mag_declination_deg * kPi / 180;
    // Reference field heading in ENU: north rotated east by the declination.
    const Scalar ref_heading = std::atan2(std::cos(decl), std::sin(decl));
    const Scalar yaw = ref_heading - std::atan2(m_level.y(), m_level.x());
    init(fix.t, p_ant, yaw, cfg_.init_sigma_yaw_mag, fix.has_vel ? fix.vel_enu : Vec3::Zero(),
         pos_sigma);
    return;
  }
#endif

  // Otherwise heading = GPS direction of travel. That is only the car's heading
  // when it drives straight and forward: in a turn the IMU slides sideways a
  // little (it sits ahead of the rear axle), and in reverse it points backwards.
  // Without wheel speed we can't see reverse, so we assume forward.
  const bool turning = std::abs(w_held_.z() - bg0_.z()) > cfg_.heading_init_max_yaw_rate;
  const bool reversing =
      have_wheel_speed_ && std::abs(fix.t - t_wheel_) < kRecentS && wheel_speed_ < 0;
  if (turning || reversing) {
    have_first_fix_ = false;  // restart the two-fix baseline below
    return;
  }

  if (fix.has_vel && fix.vel_enu.template head<2>().norm() >= cfg_.heading_init_min_speed) {
    init(fix.t, p_ant, std::atan2(fix.vel_enu.y(), fix.vel_enu.x()), cfg_.init_sigma_yaw,
         fix.vel_enu, pos_sigma);
    return;
  }
  if (!have_first_fix_) {
    have_first_fix_ = true;
    first_fix_p_ = p_ant;
  } else if (!fix.has_vel && fix.t > last_fix_t_ &&
             (p_ant - first_fix_p_).template head<2>().norm() >= cfg_.heading_init_min_dist) {
    const Vec3 d = p_ant - first_fix_p_;
    const Vec3 v0 = (p_ant - last_fix_p_) / Scalar(fix.t - last_fix_t_);
    init(fix.t, p_ant, std::atan2(d.y(), d.x()), cfg_.init_sigma_yaw, v0, pos_sigma);
    return;
  }
  last_fix_p_ = p_ant;
  last_fix_t_ = fix.t;
}

void Localizer::init(double t, const Vec3& p_ant, Scalar yaw, Scalar yaw_sigma, const Vec3& v_ant,
                     Scalar pos_sigma) {
  const Mat3 R = rot_z(yaw) * R_level_;
  NominalState x0;
  x0.R = to_so3(R);
  x0.p = p_ant - R * cfg_.gps_antenna;
  x0.v = v_ant - R * (w_held_ - bg0_).cross(cfg_.gps_antenna);  // antenna -> IMU velocity
  x0.bg = bg0_;

  VecN sd;
  sd << Vec3::Constant(pos_sigma), Vec3::Constant(cfg_.init_sigma_vel),
      cfg_.init_sigma_tilt, cfg_.init_sigma_tilt, yaw_sigma,
      Vec3::Constant(cfg_.init_sigma_accel_bias), Vec3::Constant(cfg_.init_sigma_gyro_bias),
      cfg_.init_sigma_wheel_scale, cfg_.init_sigma_steer_offset;
  // Attitude error is in the body frame; yaw sigma on body z is close enough for a car.
  eskf_.reset(x0, sd.cwiseAbs2().asDiagonal(), gravity_);
  t_filter_ = t;
  t_within_since_ = t;
  status_ = Status::Running;
}

// ---------------------------------------------------------------- corrections

bool Localizer::prepare(double t) {
  if (status_ != Status::Running) return false;
  if (t < t_filter_) {
    ++late_;  // older than data already used: fusing it now would put it at the wrong time
    return false;
  }
  if (t - t_imu_ > cfg_.max_imu_gap_s) {
    faults_ |= kFaultImuGap;  // the IMU went quiet while this sensor still talks
    return false;
  }
  propagate(t, f_held_, w_held_);  // hold the last IMU sample up to the measurement time
  return true;
}

template <int M>
Innovation Localizer::correct(double t, Sensor s, const Eigen::Matrix<Scalar, M, 1>& y,
                              const Eigen::Matrix<Scalar, M, kStates>& H,
                              const Eigen::Matrix<Scalar, M, M>& Rm) {
  const auto i = static_cast<int>(s);
  Innovation in;
  in.t = t;
  in.sensor = s;
  in.dim = M;
  in.gate = cfg_.nis_gate[M - 1];
  const UpdateResult r = eskf_.update<M>(y, H, Rm, in.gate);
  in.nis = r.nis;
  in.accepted = r.accepted;
  for (int k = 0; k < M; ++k) in.y[k] = y[k];

  // Fault only if this sensor keeps failing (see Config). GPS is the reference,
  // so N rejects in a row is enough; helper sensors must also fail for a while.
  RejectStreak& streak = rejects_[i];
  if (r.accepted) {
    streak.count = 0;
  } else {
    if (streak.count == 0) streak.since = t;
    ++streak.count;
    const bool is_gps = s == Sensor::GpsPos || s == Sensor::GpsVel;
    const bool long_enough = is_gps || t - streak.since >= cfg_.max_reject_time_s;
    if (streak.count >= cfg_.max_consecutive_rejects && long_enough) {
      faults_ |= kFaultInnovation;
    }
  }
  if (std::isfinite(r.nis)) nis_ratio_[i] += Scalar(0.05) * (r.nis / M - nis_ratio_[i]);
  check_health();
  return in;
}

GpsInnovations Localizer::on_gps(const GpsFix& fix) {
  GpsInnovations out;
  if (!have_datum_) set_datum(fix.lat_deg, fix.lon_deg, fix.alt_m);
  double e = 0, n = 0, u = 0;
  enu_.Forward(fix.lat_deg, fix.lon_deg, fix.alt_m, e, n, u);
  const Vec3 z{Scalar(e), Scalar(n), Scalar(u)};

  if (status_ == Status::WaitingForHeading) {
    try_init_from_gps(fix, z);
    return out;
  }
  if (!prepare(fix.t)) return out;

#if LOC_USE_GPS_POS
  {
    const NominalState& x = eskf_.x();
    const Mat3 R = x.R.rotation();
    const Vec3& l = cfg_.gps_antenna;
    Mat3xN H = Mat3xN::Zero();
    H.block<3, 3>(0, kP).setIdentity();
    H.block<3, 3>(0, kTh) = -R * manif::skew(l);
    const Scalar sh = std::max(fix.h_acc, cfg_.gps_pos_sigma_floor);
    const Scalar sv = std::max(fix.v_acc, cfg_.gps_pos_sigma_floor);
    out.pos = correct<3>(fix.t, Sensor::GpsPos, Vec3(z - (x.p + R * l)), H, diag3(sh, sh, sv));
  }
#endif
#if LOC_USE_GPS_VEL
  if (fix.has_vel) {
    const NominalState& x = eskf_.x();
    const Mat3 R = x.R.rotation();
    const Vec3& l = cfg_.gps_antenna;
    const Vec3 lever_vel = (w_held_ - x.bg).cross(l);  // antenna velocity from rotation
    Mat3xN H = Mat3xN::Zero();
    H.block<3, 3>(0, kV).setIdentity();
    H.block<3, 3>(0, kTh) = -R * manif::skew(lever_vel);
    H.block<3, 3>(0, kBg) = R * manif::skew(l);
    const Scalar s = std::max(fix.s_acc, cfg_.gps_vel_sigma_floor);
    out.vel = correct<3>(fix.t, Sensor::GpsVel, Vec3(fix.vel_enu - (x.v + R * lever_vel)), H,
                         diag3(s, s, s));
  }
#endif
  return out;
}

std::optional<Innovation> Localizer::on_wheel_rpm(double t, Scalar motor_rpm) {
#if LOC_USE_WHEEL_SPEED
  const Scalar speed = motor_rpm * 2 * kPi / 60 / cfg_.gear_ratio * cfg_.wheel_radius;
  have_wheel_speed_ = true;  // remembered so start-up can tell forward from reverse
  t_wheel_ = t;
  wheel_speed_ = speed;
  if (!prepare(t)) return std::nullopt;
  // Rear-axle velocity in the body frame should be [wheel speed, 0, 0]: the
  // zeros are the non-holonomic constraint (no sideways / vertical sliding).
  const NominalState& x = eskf_.x();
  const Mat3 Rt = x.R.rotation().transpose();
  const Vec3& l = cfg_.rear_axle;
  const Vec3 v_body = Rt * x.v;
  const Vec3 v_axle = v_body + (w_held_ - x.bg).cross(l);
  Mat3xN H = Mat3xN::Zero();
  H.block<3, 3>(0, kV) = Rt;
  H.block<3, 3>(0, kTh) = manif::skew(v_body);
  H.block<3, 3>(0, kBg) = manif::skew(l);
  // Forward row measures (1 + wheel_scale) * v_axle.x: tyre radius is never exact.
  H.row(0) *= 1 + x.wheel_scale;
  H(0, kSw) = v_axle.x();
  Vec3 h = v_axle;
  h.x() *= 1 + x.wheel_scale;
  return correct<3>(t, Sensor::WheelSpeed, Vec3(Vec3(speed, 0, 0) - h), H,
                    diag3(cfg_.wheel_speed_sigma, cfg_.nhc_lateral_sigma, cfg_.nhc_vertical_sigma));
#else
  (void)t, (void)motor_rpm;
  return std::nullopt;
#endif
}

std::optional<Innovation> Localizer::on_steering(double t, Scalar road_wheel_angle_rad) {
#if LOC_USE_STEERING
  if (!prepare(t)) return std::nullopt;
  // Kinematic bicycle: yaw rate = v * tan(delta) / L. Pseudo-measurement
  // 0 = (gyro_z - b_gz) - v_x * k, with the steering offset estimated (a 0.1 deg
  // offset at 10 m/s is a 0.01 rad/s yaw-rate error, bigger than the gyro bias).
  const NominalState& x = eskf_.x();
  const Mat3 Rt = x.R.rotation().transpose();
  const Vec3 v_body = Rt * x.v;
  const Scalar delta = road_wheel_angle_rad - x.steer_offset;
  const Scalar k = std::tan(delta) / cfg_.wheelbase;
  const Scalar h = (w_held_.z() - x.bg.z()) - k * v_body.x();
  Mat1xN H = Mat1xN::Zero();
  H(0, kBg + 2) = -1;
  H.block<1, 3>(0, kV) = -k * Rt.row(0);
  H.block<1, 3>(0, kTh) = -k * manif::skew(v_body).row(0);
  H(0, kSo) = v_body.x() / (cfg_.wheelbase * std::cos(delta) * std::cos(delta));
  const Scalar s = cfg_.steering_yaw_rate_sigma;
  return correct<1>(t, Sensor::Steering, Vec1(-h), H, Mat1(s * s));
#else
  (void)t, (void)road_wheel_angle_rad;
  return std::nullopt;
#endif
}

std::optional<Innovation> Localizer::on_mag(double t, const Vec3& mag_raw) {
#if LOC_USE_MAG
  const Vec3 m = cfg_.R_body_mag * (cfg_.mag_soft_iron * (mag_raw - cfg_.mag_hard_iron));
  if (m.norm() < Scalar(1e-9)) return std::nullopt;
  mag_body_ = m.normalized();
  have_mag_ = true;
  t_mag_ = t;
  if (!prepare(t)) return std::nullopt;

  // Compare field directions (unit vectors) so magnitude errors don't matter.
  const Scalar decl = cfg_.mag_declination_deg * kPi / 180;
  const Scalar incl = cfg_.mag_inclination_deg * kPi / 180;
  const Vec3 m_world(std::cos(incl) * std::sin(decl), std::cos(incl) * std::cos(decl),
                     -std::sin(incl));
  const Vec3 h = eskf_.x().R.rotation().transpose() * m_world;
  Mat3xN H = Mat3xN::Zero();
  H.block<3, 3>(0, kTh) = manif::skew(h);
  const Scalar s = cfg_.mag_sigma;
  return correct<3>(t, Sensor::Mag, Vec3(mag_body_ - h), H, diag3(s, s, s));
#else
  (void)t, (void)mag_raw;
  return std::nullopt;
#endif
}

std::optional<Innovation> Localizer::on_expected_accel(double t, Scalar accel_mps2) {
#if LOC_USE_EXPECTED_ACCEL
  if (!prepare(t)) return std::nullopt;
  // Forward kinematic acceleration = (f - b_a)_x + (R' g)_x.
  const NominalState& x = eskf_.x();
  const Vec3 g_body = x.R.rotation().transpose() * eskf_.gravity();
  const Scalar h = (f_held_.x() - x.ba.x()) + g_body.x();
  Mat1xN H = Mat1xN::Zero();
  H(0, kBa) = -1;
  H.block<1, 3>(0, kTh) = manif::skew(g_body).row(0);
  const Scalar s = cfg_.expected_accel_sigma;
  return correct<1>(t, Sensor::ExpectedAccel, Vec1(accel_mps2 - h), H, Mat1(s * s));
#else
  (void)t, (void)accel_mps2;
  return std::nullopt;
#endif
}

// ---------------------------------------------------------------- health / output

void Localizer::check_health() {
  const Cov& P = eskf_.P();
  const NominalState& x = eskf_.x();
  if (!P.allFinite() || !x.p.allFinite() || !x.v.allFinite() || !x.R.coeffs().allFinite()) {
    faults_ |= kFaultNumerical;
  }
  // Armed once the believed RMS has stayed under the limit for ready_time_s, so
  // start-up convergence reads as "not ready" rather than a latched fault.
  if (std::sqrt(P(0, 0) + P(1, 1)) <= cfg_.max_pos_rms) {
    if (t_filter_ - t_within_since_ >= cfg_.ready_time_s) converged_ = true;
  } else {
    t_within_since_ = t_filter_;
    if (converged_) faults_ |= kFaultPositionRms;
  }
}

void Localizer::clear_faults() {
  faults_ = 0;
  rejects_.fill({});
  converged_ = false;  // earn `ready` again
  t_within_since_ = t_filter_;
}

Output Localizer::output() const { return make_output(eskf_.x(), t_filter_); }

Output Localizer::predict_output(std::span<const ImuSample> newer_imu) const {
  if (status_ != Status::Running) return output();
  NominalState x = eskf_.x();
  double t = t_filter_;
  for (const ImuSample& s : newer_imu) {
    if (s.t <= t) continue;
    Eskf::integrate(x, cfg_.R_body_imu * s.accel, cfg_.R_body_imu * s.gyro, Scalar(s.t - t),
                    eskf_.gravity());
    t = s.t;
  }
  return make_output(x, t);
}

Output Localizer::make_output(const NominalState& x, double t) const {
  Output o;
  o.t = status_ == Status::Running ? t : t_imu_;
  o.status = status_;
  o.faults = faults_;
  o.nis_ratio = nis_ratio_;
  o.late_measurements = late_;
  o.ready = status_ == Status::Running && converged_ && faults_ == 0;
  if (status_ != Status::Running) return o;

  const Mat3 R = x.R.rotation();
  o.p_enu = x.p;
  o.v_enu = x.v;
  o.rpy = Vec3(std::atan2(R(2, 1), R(2, 2)), std::asin(-R(2, 0)), std::atan2(R(1, 0), R(0, 0)));
  o.accel_bias = x.ba;
  o.gyro_bias = x.bg;
  enu_.Reverse(double(x.p.x()), double(x.p.y()), double(x.p.z()), o.lat_deg, o.lon_deg, o.alt_m);
  const Cov& P = eskf_.P();
  o.pos_sigma = P.diagonal().segment<3>(kP).cwiseSqrt();
  o.pos_rms_h = std::sqrt(P(0, 0) + P(1, 1));
  return o;
}

}  // namespace loc
