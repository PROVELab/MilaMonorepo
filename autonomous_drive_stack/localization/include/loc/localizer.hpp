#pragma once
// Localizer: owns the ESKF and turns raw, asynchronous sensor readings into
// corrections. Call the on_*() functions in timestamp order from one task.
//
//   IMU (required, ~416-450 Hz)  -> prediction
//   everything else (any rate)   -> opt-in corrections, applied at their own
//                                   timestamp (the filter is propagated to it)
//
// Every correction returns its innovation. Faults latch until clear_faults();
// any fault means "stop the vehicle".
//
// Readings older than data the filter has already used are dropped (counted in
// late_measurements). On the car, use LiveLocalizer (live_localizer.hpp), which
// puts late readings back in time order first.

#include "loc/eskf.hpp"

#include <GeographicLib/LocalCartesian.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace loc {

enum Fault : std::uint32_t {
  kFaultInnovation = 1u << 0,   // a sensor kept failing the innovation gate (see Config)
  kFaultPositionRms = 1u << 1,  // believed horizontal RMS error > max_pos_rms (after converging)
  kFaultImuGap = 1u << 2,       // IMU samples stopped or jumped (other sensors still talking)
  kFaultNumerical = 1u << 3,    // NaN/Inf in state or covariance
};

enum class Sensor : std::uint8_t { GpsPos, GpsVel, WheelSpeed, Steering, Mag, ExpectedAccel };
inline constexpr int kNumSensors = 6;
inline constexpr std::array<const char*, kNumSensors> kSensorNames = {
    "gps_pos", "gps_vel", "wheel_speed", "steering", "mag", "expected_accel"};

enum class Status : std::uint8_t {
  Aligning,           // waiting for a stationary period to level + estimate gyro bias
  WaitingForHeading,  // need GPS (+ motion, or magnetometer) to set position and yaw
  Running,
};

struct ImuSample {
  double t = 0;
  Vec3 accel = Vec3::Zero(), gyro = Vec3::Zero();  // IMU frame, m/s^2 and rad/s
};

struct GpsFix {
  double t = 0;  // validity time on the common clock, s
  double lat_deg = 0, lon_deg = 0, alt_m = 0;  // WGS84; alt = ellipsoidal height
  Scalar h_acc = 0, v_acc = 0;  // receiver 1-sigma accuracy, m (u-blox hAcc/vAcc)
  bool has_vel = false;
  Vec3 vel_enu = Vec3::Zero();  // m/s
  Scalar s_acc = 0;             // m/s
};

struct Innovation {
  double t = 0;
  Sensor sensor{};
  int dim = 0;
  Scalar nis = 0;   // y' S^-1 y ; expected value = dim
  Scalar gate = 0;  // rejected if nis > gate
  bool accepted = false;
  std::array<Scalar, 3> y{};  // z - h(x), first `dim` entries valid
};

struct GpsInnovations {
  std::optional<Innovation> pos, vel;
};

struct Output {
  double t = 0;  // time the state is valid. If now - t > max_imu_gap_s, the localizer
                 // has stopped running: treat the output as not ready.
  Status status = Status::Aligning;
  bool ready = false;  // running, under max_pos_rms for ready_time_s, no faults: OK to drive
  std::uint32_t faults = 0;
  Vec3 p_enu = Vec3::Zero(), v_enu = Vec3::Zero();
  Vec3 rpy = Vec3::Zero();  // ZYX roll, pitch, yaw (rad); yaw 0 = east, CCW+
  Vec3 accel_bias = Vec3::Zero(), gyro_bias = Vec3::Zero();
  double lat_deg = 0, lon_deg = 0, alt_m = 0;  // absolute position of the IMU
  Vec3 pos_sigma = Vec3::Zero();  // 1-sigma E, N, U the filter believes, m
  Scalar pos_rms_h = 0;           // sqrt(var_E + var_N): believed horizontal RMS error
  // "Error of the error": running mean of NIS/dim per sensor. ~1 when the
  // filter's uncertainty is honest; >>1 means it is overconfident.
  std::array<Scalar, kNumSensors> nis_ratio{};
  std::uint32_t late_measurements = 0;
};

class Localizer {
 public:
  explicit Localizer(const Config& cfg = {});

  // Optional fixed ENU datum. Otherwise the first GPS fix becomes the datum.
  void set_datum(double lat_deg, double lon_deg, double alt_m);

  void on_imu(double t, const Vec3& accel_mps2, const Vec3& gyro_rps);  // IMU frame
  GpsInnovations on_gps(const GpsFix& fix);
  std::optional<Innovation> on_wheel_rpm(double t, Scalar motor_rpm);
  std::optional<Innovation> on_steering(double t, Scalar road_wheel_angle_rad);
  std::optional<Innovation> on_mag(double t, const Vec3& mag_raw);  // any unit
  std::optional<Innovation> on_expected_accel(double t, Scalar accel_mps2);

  Output output() const;
  // The output pushed forward through IMU samples the filter hasn't used yet
  // (prediction only: no corrections, covariance not grown). For LiveLocalizer.
  Output predict_output(std::span<const ImuSample> newer_imu) const;
  Status status() const { return status_; }
  std::uint32_t faults() const { return faults_; }
  // Clears latched faults. `ready` then has to be earned again (believed RMS
  // under max_pos_rms for ready_time_s). After kFaultNumerical the state is
  // garbage: construct a new Localizer instead.
  void clear_faults();

 private:
  void align(double t, const Vec3& f, const Vec3& w);
  void try_init_from_gps(const GpsFix& fix, const Vec3& p_ant);
  void init(double t, const Vec3& p_ant, Scalar yaw, Scalar yaw_sigma, const Vec3& v_ant,
            Scalar pos_sigma);
  bool prepare(double t);  // propagate to t; false if not running or too late
  void propagate(double t, const Vec3& f, const Vec3& w);
  template <int M>
  Innovation correct(double t, Sensor s, const Eigen::Matrix<Scalar, M, 1>& y,
                     const Eigen::Matrix<Scalar, M, kStates>& H,
                     const Eigen::Matrix<Scalar, M, M>& Rm);
  void check_health();
  Output make_output(const NominalState& x, double t) const;

  Config cfg_;
  Eskf eskf_;
  Status status_ = Status::Aligning;
  std::uint32_t faults_ = 0;

  GeographicLib::LocalCartesian enu_;
  bool have_datum_ = false;
  Vec3 gravity_ = Vec3(0, 0, -9.80665);

  // IMU bookkeeping (body frame). The ESKF state is valid at t_filter_.
  bool have_imu_ = false;
  double t_imu_ = 0, t_filter_ = 0;
  Vec3 f_held_ = Vec3::Zero(), w_held_ = Vec3::Zero();

  // Alignment
  int n_align_ = 0;
  double t_align0_ = 0;
  Vec3 sum_f_ = Vec3::Zero(), sum_w_ = Vec3::Zero();
  Mat3 R_level_ = Mat3::Identity();  // roll/pitch only
  Vec3 bg0_ = Vec3::Zero();

  // Heading initialisation
  bool have_first_fix_ = false;
  Vec3 first_fix_p_ = Vec3::Zero(), last_fix_p_ = Vec3::Zero();
  double last_fix_t_ = 0;
  bool have_wheel_speed_ = false;  // latest wheel speed, so start-up can tell reverse
  double t_wheel_ = 0;
  Scalar wheel_speed_ = 0;
  bool converged_ = false;  // believed RMS stayed below max_pos_rms for ready_time_s
  double t_within_since_ = 0;
#if LOC_USE_MAG
  bool have_mag_ = false;
  double t_mag_ = 0;
  Vec3 mag_body_ = Vec3::Zero();  // unit vector
#endif

  struct RejectStreak {  // rejections in a row from one sensor
    int count = 0;
    double since = 0;  // time of the first one
  };
  std::array<RejectStreak, kNumSensors> rejects_{};
  std::array<Scalar, kNumSensors> nis_ratio_{};
  std::uint32_t late_ = 0;
};

}  // namespace loc
