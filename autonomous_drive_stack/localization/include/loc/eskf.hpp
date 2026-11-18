#pragma once
// Error-state EKF for IMU-driven navigation (Sola, "Quaternion kinematics for
// the error-state Kalman filter", 2017, local/right attitude error).
//
// Nominal state  x = [p, v, R, b_a, b_g, s_w, d_0]  integrated from the IMU every sample.
// Error state   dx = [dp, dv, dth, db_a, db_g, ds_w, dd_0] (17), carries the covariance P.
// s_w (wheel-speed scale error) and d_0 (steering offset) are car calibrations:
// systematic errors that would otherwise make the filter overconfident.
// A correction estimates dx, injects it into x, and resets dx to zero.
// No heap allocation, no exceptions: safe for FreeRTOS tasks.

#include "loc/config.hpp"

#include <Eigen/Cholesky>
#include <Eigen/Core>
#include <cassert>  // manif uses assert() but relies on Eigen < 5 to include this
#include <manif/SO3.h>

namespace loc {

using SO3 = manif::SO3<Scalar>;
using SO3Tangent = manif::SO3Tangent<Scalar>;

inline constexpr int kStates = 17;
enum : int { kP = 0, kV = 3, kTh = 6, kBa = 9, kBg = 12, kSw = 15, kSo = 16 };  // offsets

using VecN = Eigen::Matrix<Scalar, kStates, 1>;
using Cov = Eigen::Matrix<Scalar, kStates, kStates>;

struct NominalState {
  Vec3 p = Vec3::Zero();   // ENU position of the IMU, m
  Vec3 v = Vec3::Zero();   // ENU velocity, m/s
  SO3 R = SO3::Identity(); // body -> world rotation
  Vec3 ba = Vec3::Zero();  // accelerometer bias, m/s^2
  Vec3 bg = Vec3::Zero();  // gyro bias, rad/s
  Scalar wheel_scale = 0;  // measured speed = (1 + wheel_scale) * true speed
  Scalar steer_offset = 0; // true road-wheel angle = measured - steer_offset, rad
};

struct UpdateResult {
  bool accepted = false;
  Scalar nis = 0;  // normalised innovation squared, y' S^-1 y
};

class Eskf {
 public:
  void reset(const NominalState& x0, const Cov& P0, const Vec3& gravity);

  // Propagate with body-frame specific force f [m/s^2] and rate w [rad/s].
  void predict(const Vec3& f, const Vec3& w, Scalar dt, const Config& cfg);

  // Just the nominal-state part of predict() (no covariance), for any state.
  static void integrate(NominalState& x, const Vec3& f, const Vec3& w, Scalar dt,
                        const Vec3& gravity);

  // Correct with innovation y = z - h(x) and its error-state Jacobian H.
  // Rejected (state untouched) when NIS > gate.
  template <int M>
  UpdateResult update(const Eigen::Matrix<Scalar, M, 1>& y,
                      const Eigen::Matrix<Scalar, M, kStates>& H,
                      const Eigen::Matrix<Scalar, M, M>& Rm, Scalar gate);

  const NominalState& x() const { return x_; }
  const Cov& P() const { return P_; }
  const Vec3& gravity() const { return g_; }

 private:
  void inject(const VecN& dx);

  NominalState x_;
  Cov P_ = Cov::Identity();
  Vec3 g_ = Vec3(0, 0, -9.80665);
};

template <int M>
UpdateResult Eskf::update(const Eigen::Matrix<Scalar, M, 1>& y,
                          const Eigen::Matrix<Scalar, M, kStates>& H,
                          const Eigen::Matrix<Scalar, M, M>& Rm, Scalar gate) {
  using MatM = Eigen::Matrix<Scalar, M, M>;
  const Eigen::Matrix<Scalar, kStates, M> PHt = P_ * H.transpose();
  const MatM S = H * PHt + Rm;
  const Eigen::LDLT<MatM> S_ldlt(S);

  UpdateResult r;
  r.nis = y.dot(S_ldlt.solve(y));
  if (!(r.nis <= gate)) return r;  // also rejects NaN

  // K = P H' S^-1  (S symmetric, so K' = S^-1 H P)
  const Eigen::Matrix<Scalar, kStates, M> K = S_ldlt.solve(PHt.transpose()).transpose();
  const Cov IKH = Cov::Identity() - K * H;
  P_ = IKH * P_ * IKH.transpose() + K * Rm * K.transpose();  // Joseph form
  inject(K * y);
  r.accepted = true;
  return r;
}

}  // namespace loc
