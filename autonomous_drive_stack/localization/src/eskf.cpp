#include "loc/eskf.hpp"

namespace loc {

void Eskf::reset(const NominalState& x0, const Cov& P0, const Vec3& gravity) {
  x_ = x0;
  P_ = P0;
  g_ = gravity;
}

void Eskf::integrate(NominalState& x, const Vec3& f, const Vec3& w, Scalar dt,
                     const Vec3& gravity) {
  const Vec3 a_b = f - x.ba;
  const Vec3 w_b = w - x.bg;
  // Rotate the accel with the mid-interval attitude (avoids a drift in long turns)
  const Vec3 a_w = (x.R * SO3Tangent(w_b * (dt / 2)).exp()).act(a_b) + gravity;
  // Biases are constant between corrections
  x.p += x.v * dt + Scalar(0.5) * a_w * dt * dt;
  x.v += a_w * dt;
  x.R = x.R * SO3Tangent(w_b * dt).exp();
}

void Eskf::predict(const Vec3& f, const Vec3& w, Scalar dt, const Config& cfg) {
  // F below needs the attitude and the bias-corrected IMU values from before the step
  const Vec3 a_b = f - x_.ba;
  const Vec3 w_b = w - x_.bg;
  const Mat3 R = x_.R.rotation();
  const SO3 dR = SO3Tangent(w_b * dt).exp();
  integrate(x_, f, w, dt, g_);

  // Error-state transition (Sola eq. 269)
  Cov F = Cov::Identity();
  F.block<3, 3>(kP, kV) = Mat3::Identity() * dt;
  F.block<3, 3>(kV, kTh) = -R * manif::skew(a_b) * dt;
  F.block<3, 3>(kV, kBa) = -R * dt;
  F.block<3, 3>(kTh, kTh) = dR.rotation().transpose();
  F.block<3, 3>(kTh, kBg) = -Mat3::Identity() * dt;

  // Process noise: densities^2 * dt
  VecN q;
  q.segment<3>(kP).setZero();
  q.segment<3>(kV).setConstant(cfg.accel_noise_density * cfg.accel_noise_density * dt);
  q.segment<3>(kTh).setConstant(cfg.gyro_noise_density * cfg.gyro_noise_density * dt);
  q.segment<3>(kBa).setConstant(cfg.accel_bias_rw * cfg.accel_bias_rw * dt);
  q.segment<3>(kBg).setConstant(cfg.gyro_bias_rw * cfg.gyro_bias_rw * dt);
  q(kSw) = cfg.wheel_scale_rw * cfg.wheel_scale_rw * dt;
  q(kSo) = cfg.steer_offset_rw * cfg.steer_offset_rw * dt;

  P_ = F * P_ * F.transpose();
  P_.diagonal() += q;
}

void Eskf::inject(const VecN& dx) {
  x_.p += dx.segment<3>(kP);
  x_.v += dx.segment<3>(kV);
  x_.R = x_.R + SO3Tangent(dx.segment<3>(kTh));  // right-plus: R * Exp(dth)
  x_.ba += dx.segment<3>(kBa);
  x_.bg += dx.segment<3>(kBg);
  x_.wheel_scale += dx(kSw);
  x_.steer_offset += dx(kSo);

  // Reset the error to zero; its covariance rotates slightly (Sola eq. 287)
  Cov G = Cov::Identity();
  G.block<3, 3>(kTh, kTh) -= manif::skew(Vec3(Scalar(0.5) * dx.segment<3>(kTh)));
  P_ = G * P_ * G.transpose();
  P_ = Scalar(0.5) * (P_ + P_.transpose());
}

}  // namespace loc
