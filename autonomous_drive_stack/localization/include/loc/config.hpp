#pragma once
// ============================================================================
// Localization config: compile-time switches + tunable parameters.
//
// Every correction below is opt-in twice over:
//   1. compile-time: set its LOC_USE_* macro to 0 to compile it out entirely;
//   2. run-time:     a correction only happens if you feed that sensor in.
// The IMU is the only required input (it drives the prediction).
// ============================================================================

#ifndef LOC_SCALAR
#define LOC_SCALAR double   // use float on the ESP32 (its FPU is single precision)
#endif
#ifndef LOC_USE_GPS_POS
#define LOC_USE_GPS_POS 1          // GPS / RTK position
#endif
#ifndef LOC_USE_GPS_VEL
#define LOC_USE_GPS_VEL 1          // GPS velocity (u-blox NAV-PVT gives it with the fix)
#endif
#ifndef LOC_USE_WHEEL_SPEED
#define LOC_USE_WHEEL_SPEED 1      // inverter rpm -> speed, + "car can't slide sideways"
#endif
#ifndef LOC_USE_STEERING
#define LOC_USE_STEERING 1         // steering angle -> yaw rate (kinematic bicycle)
#endif
#ifndef LOC_USE_MAG
#define LOC_USE_MAG 0              // magnetometer heading (likely disturbed by the motor)
#endif
#ifndef LOC_USE_EXPECTED_ACCEL
#define LOC_USE_EXPECTED_ACCEL 0   // accel expected from inverter/brake commands (crude;
                                   // doubled the error in a GPS outage, see results)
#endif

#include <Eigen/Core>
#include <array>
#include <cstdint>

namespace loc {

using Scalar = LOC_SCALAR;
using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
using Mat3 = Eigen::Matrix<Scalar, 3, 3>;

// Frames: vehicle body = x forward, y left, z up (FLU). World = local ENU
// (east, north, up) tangent plane at the datum. Body origin = the IMU.
struct Config {
  // ---- IMU noise (continuous-time densities) ----
  // ISM330DHCX datasheet: accel ~60-80 ug/sqrt(Hz), gyro 5 mdps/sqrt(Hz).
  // Values here are inflated ~2x to cover vibration and model error.
  Scalar accel_noise_density = 1.6e-3;  // m/s^2/sqrt(Hz)
  Scalar gyro_noise_density = 1.7e-4;   // rad/s/sqrt(Hz)
  Scalar accel_bias_rw = 2e-4;          // m/s^3/sqrt(Hz)  bias random walk
  Scalar gyro_bias_rw = 2e-5;           // rad/s^2/sqrt(Hz)
  Scalar wheel_scale_rw = 1e-5;         // 1/sqrt(s)   (tyre wear, temperature)
  Scalar steer_offset_rw = 1e-5;        // rad/sqrt(s)

  // ---- Mounting (PLACEHOLDERS: measure on the car) ----
  Mat3 R_body_imu = Mat3::Identity();   // rotates IMU-frame vectors into body frame
  Mat3 R_body_mag = Mat3::Identity();   // MMC5983MA has its own axes
  Vec3 gps_antenna = {0.0, 0.0, 0.8};   // antenna position in body frame, m
  Vec3 rear_axle = {-0.8, 0.0, -0.2};   // rear-axle centre in body frame, m

  // ---- Vehicle (PLACEHOLDERS) ----
  Scalar wheel_radius = 0.23;  // m
  Scalar gear_ratio = 4.0;     // motor revs per wheel rev
  Scalar wheelbase = 1.55;     // m

  // ---- Measurement noise (1-sigma) ----
  Scalar gps_pos_sigma_floor = 0.01;  // m, floor applied to receiver hAcc/vAcc
  Scalar gps_vel_sigma_floor = 0.02;  // m/s, floor applied to receiver sAcc
  Scalar wheel_speed_sigma = 0.10;    // m/s forward (slip, tyre radius error)
  Scalar nhc_lateral_sigma = 0.10;    // m/s sideways at the rear axle
  Scalar nhc_vertical_sigma = 0.10;   // m/s vertical at the rear axle
  Scalar steering_yaw_rate_sigma = 0.03;  // rad/s, bicycle-model yaw rate
  Scalar mag_sigma = 0.08;            // unit-vector components (~5 deg)
  Scalar expected_accel_sigma = 0.6;  // m/s^2 (commands -> accel is crude)

  // ---- Magnetometer calibration + local field (San Luis Obispo, WMM) ----
  Vec3 mag_hard_iron = Vec3::Zero();
  Mat3 mag_soft_iron = Mat3::Identity();
  Scalar mag_declination_deg = 11.6;  // east positive
  Scalar mag_inclination_deg = 59.0;  // down positive

  // ---- Initialisation ----
  double align_time_s = 2.0;          // stationary time to level + estimate gyro bias
  Scalar align_max_gyro = 0.05;       // rad/s; above this we're not stationary
  Scalar align_max_accel_dev = 0.3;   // m/s^2 deviation of |a| from g
  Scalar heading_init_min_speed = 2.0;  // m/s of GPS velocity before heading from course
  Scalar heading_init_min_dist = 3.0;   // m between fixes if no GPS velocity
  // GPS direction of travel = heading only when driving straight and forward.
  Scalar heading_init_max_yaw_rate = 0.05;  // rad/s; above this we're turning
  Scalar init_sigma_vel = 0.2;        // m/s
  Scalar init_sigma_tilt = 0.02;      // rad (roll, pitch)
  Scalar init_sigma_yaw = 0.05;       // rad, heading from GPS course
  Scalar init_sigma_yaw_mag = 0.2;    // rad, heading from magnetometer
  Scalar init_sigma_accel_bias = 0.1; // m/s^2
  Scalar init_sigma_gyro_bias = 0.005; // rad/s (after static averaging)
  Scalar init_sigma_wheel_scale = 0.02;   // 2% tyre-radius / gear uncertainty
  Scalar init_sigma_steer_offset = 0.02;  // rad (~1 deg) steering calibration

  // ---- Faults (any fault => stop the vehicle) ----
  // Innovation gate = chi-square 99.9% for the measurement's dimension. A
  // measurement above it is rejected (skipped). Then:
  //  * GPS is the reference: `max_consecutive_rejects` rejected fixes in a row
  //    raise kFaultInnovation (we no longer know where we are).
  //  * Helper sensors (wheel speed, steering, mag, expected accel) can be briefly
  //    wrong, e.g. wheel spin at launch. They raise it only if they also keep
  //    failing for `max_reject_time_s`, so slip is skipped but a broken sensor
  //    still stops the car.
  std::array<Scalar, 3> nis_gate = {10.83, 13.82, 16.27};  // dof 1, 2, 3
  int max_consecutive_rejects = 3;
  double max_reject_time_s = 1.0;  // helper sensors only
  Scalar max_pos_rms = 0.1524;  // m (6 in): horizontal RMS the filter believes
  double ready_time_s = 3.0;    // must stay under max_pos_rms this long before it's armed
  double max_imu_gap_s = 0.05;  // s; also: a reading this much newer than the last IMU sample

  // ---- Live use (LiveLocalizer) ----
  // How long to hold readings so late ones (GPS arrives ~50-100 ms after the
  // moment it describes) can still be used in time order.
  double fusion_delay_s = 0.15;
};

}  // namespace loc
