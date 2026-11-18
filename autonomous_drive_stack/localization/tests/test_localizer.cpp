// Tests for the safety logic. Each test drives a tiny, noise-free synthetic car
// through one situation the red-team review found a problem with.
//
//   cmake --build build && ctest --test-dir build --output-on-failure

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "loc/live_localizer.hpp"

#include <GeographicLib/LocalCartesian.hpp>
#include <GeographicLib/NormalGravity.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace loc;

namespace {

constexpr double kLat = 35.3, kLon = -120.6625, kAlt = 100;  // datum
constexpr double kImuRate = 400, kGpsEvery = 40, kRpmEvery = 4;  // 400 Hz, 10 Hz, 100 Hz

// A car on flat ground, nose pointing east. Parked until t = 3 s, then speeds up
// at 2 m/s^2 to 5 m/s and cruises. direction = -1 drives it backwards (west).
struct Car {
  double direction = 1;

  double speed(double t) const { return direction * std::clamp(2 * (t - 3), 0.0, 5.0); }
  double accel(double t) const { return (t > 3 && t <= 5.5) ? 2 * direction : 0; }
  double east(double t) const {
    if (t <= 3) return 0;
    if (t <= 5.5) return direction * (t - 3) * (t - 3);
    return direction * (6.25 + 5 * (t - 5.5));
  }
};

// What the sensors would read for that car (exact, no noise).
struct Sensors {
  Car car;
  Config cfg;
  GeographicLib::LocalCartesian enu{kLat, kLon, kAlt};
  Vec3 gravity;  // same WGS84 normal gravity the Localizer uses

  Sensors() {
    double gy = 0, gz = 0;
    GeographicLib::NormalGravity::WGS84().Gravity(kLat, kAlt, gy, gz);
    gravity = Vec3(0, gy, gz);
  }
  // Specific force = acceleration - gravity (body axes = east, north, up here).
  Vec3 accel(double t) const { return Vec3(car.accel(t), 0, 0) - gravity; }
  GpsFix gps(double t) const {
    GpsFix fix;
    fix.t = t;
    enu.Reverse(car.east(t), 0, cfg.gps_antenna.z(), fix.lat_deg, fix.lon_deg, fix.alt_m);
    fix.h_acc = 0.02;
    fix.v_acc = 0.03;
    fix.has_vel = true;
    fix.vel_enu = Vec3(car.speed(t), 0, 0);
    fix.s_acc = 0.05;
    return fix;
  }
  double rpm(double t) const {
    return car.speed(t) / cfg.wheel_radius * cfg.gear_ratio * 60 / (2 * std::numbers::pi);
  }
};

// What to send while driving. Defaults: every sensor, exact values.
struct Feed {
  bool imu = true, gps = true, rpm = true;
  double gyro_z = 0;           // pretend the car is turning (rad/s)
  double rpm_scale = 1;        // wheel speed reads this times the truth (slip, broken sensor)
  double gps_jump = 0;         // GPS reads this many metres too far north (bad fix)
  double gps_latency = 0;      // GPS arrives this long after it was valid
};

// Drive from t0 to t1 (s), sending readings in arrival order.
template <class Filter>
void drive(Filter& f, const Sensors& s, double t0, double t1, const Feed& feed = {}) {
  const int k0 = int(std::lround(t0 * kImuRate)), k1 = int(std::lround(t1 * kImuRate));
  const int late_steps = int(std::lround(feed.gps_latency * kImuRate));
  for (int k = k0; k < k1; ++k) {
    const double t = k / kImuRate;
    if (feed.imu) f.on_imu(t, s.accel(t), Vec3(0, 0, feed.gyro_z));
    if (feed.rpm && k % int(kRpmEvery) == 0) f.on_wheel_rpm(t, feed.rpm_scale * s.rpm(t));
    const int kg = k - late_steps;  // the fix arriving now was valid at step kg
    if (feed.gps && kg >= 0 && kg % int(kGpsEvery) == 0) {
      GpsFix fix = s.gps(kg / kImuRate);
      fix.lat_deg += feed.gps_jump / 111e3;
      f.on_gps(fix);
    }
  }
}

template <class Filter>
double east_error(const Filter& f, const Sensors& s) {
  const Output o = f.output();
  return std::hypot(o.p_enu.x() - s.car.east(o.t), o.p_enu.y());
}

}  // namespace

TEST_CASE("normal drive: starts up, becomes ready, stays accurate, no faults") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 20);
  const Output o = L.output();
  CHECK(o.ready);
  CHECK(o.faults == 0);
  CHECK(o.late_measurements == 0);
  CHECK(east_error(L, s) < 0.01);
}

TEST_CASE("IMU stops while other sensors keep talking -> IMU gap fault, not ready") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 20);
  REQUIRE(L.output().ready);
  drive(L, s, 20, 20.5, {.imu = false});  // GPS + wheel speed only
  CHECK((L.faults() & kFaultImuGap) != 0);
  CHECK_FALSE(L.output().ready);
}

TEST_CASE("a reading older than data already used is dropped and counted, not fused") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 20);
  const double t_now = L.output().t;
  CHECK_FALSE(L.on_wheel_rpm(t_now - 0.003, s.rpm(t_now)).has_value());  // 3 ms late
  CHECK(L.output().late_measurements == 1);
}

TEST_CASE("live: GPS arriving 80 ms late is still used (LiveLocalizer)") {
  Sensors s;
  const Feed late_gps{.gps_latency = 0.08};

  LiveLocalizer live(s.cfg);
  drive(live, s, 0, 20, late_gps);
  const Output o = live.output();
  CHECK(o.ready);
  CHECK(o.faults == 0);
  CHECK(o.late_measurements == 0);
  CHECK(o.t == doctest::Approx(20 - 1 / kImuRate));  // output is current, not 0.15 s behind
  CHECK(east_error(live, s) < 0.01);

  // Without the delay buffer every late fix after start-up would be dropped.
  Localizer plain(s.cfg);
  drive(plain, s, 0, 20, late_gps);
  CHECK(plain.output().late_measurements > 100);
}

TEST_CASE("live: IMU stops while other sensors keep talking -> IMU gap fault") {
  Sensors s;
  LiveLocalizer live(s.cfg);
  drive(live, s, 0, 20);
  REQUIRE(live.output().ready);
  drive(live, s, 20, 20.5, {.imu = false});
  CHECK((live.faults() & kFaultImuGap) != 0);
  CHECK_FALSE(live.output().ready);
}

TEST_CASE("wheel slip for 0.3 s is skipped, a dead wheel sensor still raises a fault") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 20);
  REQUIRE(L.output().ready);

  drive(L, s, 20, 20.3, {.rpm_scale = 1.4});  // wheels spin 40% fast (old code: fault in 30 ms)
  CHECK(L.faults() == 0);
  drive(L, s, 20.3, 22);
  CHECK(L.output().ready);

  drive(L, s, 22, 24, {.rpm_scale = 0});  // sensor reads 0 while the car drives
  CHECK((L.faults() & kFaultInnovation) != 0);
  CHECK_FALSE(L.output().ready);
}

TEST_CASE("GPS is the reference: 1 bad fix is skipped, 3 in a row stop the car") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 20);
  drive(L, s, 20, 20.05, {.gps_jump = 1.5});  // one fix 1.5 m off
  drive(L, s, 20.05, 22);
  CHECK(L.faults() == 0);
  drive(L, s, 22, 22.25, {.gps_jump = 1.5});  // three fixes (0.2 s at 10 Hz)
  CHECK((L.faults() & kFaultInnovation) != 0);
}

TEST_CASE("clear_faults: the filter has to earn `ready` again") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 20);
  drive(L, s, 20, 22, {.rpm_scale = 0});
  REQUIRE(L.faults() != 0);

  drive(L, s, 22, 22.5);  // sensor fixed
  L.clear_faults();
  CHECK(L.faults() == 0);
  CHECK_FALSE(L.output().ready);  // not straight back to ready
  drive(L, s, 22.5, 22.6, {.rpm_scale = 1.4});  // one short bad burst: no instant re-fault
  CHECK(L.faults() == 0);
  drive(L, s, 22.6, 27);
  CHECK(L.output().ready);
}

TEST_CASE("start-up: no heading from GPS while turning") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 3);                   // parked: level + gyro bias
  drive(L, s, 3, 8, {.gyro_z = 0.2});  // moving at > 2 m/s, but turning
  CHECK(L.status() == Status::WaitingForHeading);
  drive(L, s, 8, 9);  // straight
  CHECK(L.status() == Status::Running);
}

TEST_CASE("start-up: no heading from GPS while reversing (wheel speed says so)") {
  Sensors s;
  s.car.direction = -1;  // nose east, driving west
  Localizer L(s.cfg);
  drive(L, s, 0, 8);
  CHECK(L.status() == Status::WaitingForHeading);  // old code: started 180 deg off
}

TEST_CASE("start-up without GPS velocity: two fixes with the same time don't divide by zero") {
  Sensors s;
  Localizer L(s.cfg);
  drive(L, s, 0, 2.5, {.gps = false});  // level + gyro bias
  REQUIRE(L.status() == Status::WaitingForHeading);
  GpsFix a = s.gps(2.5), b = s.gps(2.5);
  a.has_vel = b.has_vel = false;
  L.on_gps(a);
  L.on_gps(a);               // second fix sets the "last fix" time to 2.5
  b.lat_deg += 4 / 111e3;    // 4 m north of the first fix, same timestamp
  L.on_gps(b);
  const Output o = L.output();
  CHECK(o.v_enu.allFinite());
  CHECK((L.faults() & kFaultNumerical) == 0);
}
