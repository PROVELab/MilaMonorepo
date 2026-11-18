// Replay logged sensor data through the LiveLocalizer, as the car would run it.
//
//   replay -o out/run [--sensors imu,gps,gpsvel,rpm,steer,mag,accel] [--datum lat,lon,alt]
//          [--est-every N] [--gps-latency S] [--set name=value ...] log1.csv [log2.csv ...]
//
// --set overrides any scalar in loc::Config (see config.hpp), e.g.
//   --set accel_noise_density=0.05 --set gps_antenna=0,0,0.8
// --gps-latency S delivers each GPS fix S seconds after it was valid (as on the car).
// --sensors defaults to every correction compiled into this build. Asking for one
// that config.hpp compiles out is an error: use replay_all to compare those.
//
// Input: one event per line, any number of files (merged and sorted by time).
//   imu,t,ax,ay,az,gx,gy,gz                      m/s^2, rad/s, IMU frame
//   gps,t,lat,lon,alt,h_acc,v_acc[,ve,vn,vu,s_acc]  deg, m (ellipsoidal), m/s
//   rpm,t,motor_rpm
//   steer,t,road_wheel_angle_rad
//   mag,t,mx,my,mz                               any unit
//   accel,t,expected_accel_mps2
// Lines starting with '#', "type", or "truth" are ignored.
// Sensor "gpsinit" feeds GPS only until the filter is initialised (IMU-only run).
//
// Output: <prefix>_est.csv (state every N IMU samples) and <prefix>_innov.csv.

#include "loc/live_localizer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class Kind { Imu, Gps, Rpm, Steer, Mag, Accel };

struct Event {
  double t;        // when the reading was valid
  double arrival;  // when it reaches the filter (t, or later with --gps-latency)
  Kind kind;
  std::vector<double> v;  // fields after t
};

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  for (std::string tok; std::getline(ss, tok, sep);) out.push_back(tok);
  return out;
}

bool load(const char* path, std::vector<Event>& events) {
  std::ifstream in(path);
  if (!in) return false;
  static const std::pair<const char*, Kind> kinds[] = {
      {"imu", Kind::Imu}, {"gps", Kind::Gps}, {"rpm", Kind::Rpm},
      {"steer", Kind::Steer}, {"mag", Kind::Mag}, {"accel", Kind::Accel}};
  for (std::string line; std::getline(in, line);) {
    if (line.empty() || line[0] == '#') continue;
    const auto f = split(line, ',');
    if (f.size() < 3) continue;
    for (const auto& [name, kind] : kinds) {
      if (f[0] != name) continue;
      const double t = std::strtod(f[1].c_str(), nullptr);
      Event e{t, t, kind, {}};
      for (size_t i = 2; i < f.size(); ++i) e.v.push_back(std::strtod(f[i].c_str(), nullptr));
      events.push_back(std::move(e));
    }
  }
  return true;
}

// Override one Config field from "name=value" (3-vectors as "x,y,z").
bool set_config(loc::Config& c, const std::string& kv) {
  const auto eq = kv.find('=');
  if (eq == std::string::npos) return false;
  const std::string key = kv.substr(0, eq), val = kv.substr(eq + 1);
  const auto v = split(val, ',');
#define LOC_SCALAR_FIELD(f) \
  if (key == #f) { c.f = decltype(c.f)(std::strtod(val.c_str(), nullptr)); return true; }
#define LOC_VEC3_FIELD(f) \
  if (key == #f && v.size() == 3) { \
    c.f = loc::Vec3(std::strtod(v[0].c_str(), nullptr), std::strtod(v[1].c_str(), nullptr), \
                    std::strtod(v[2].c_str(), nullptr)); \
    return true; \
  }
  LOC_SCALAR_FIELD(accel_noise_density) LOC_SCALAR_FIELD(gyro_noise_density)
  LOC_SCALAR_FIELD(accel_bias_rw) LOC_SCALAR_FIELD(gyro_bias_rw)
  LOC_SCALAR_FIELD(wheel_scale_rw) LOC_SCALAR_FIELD(steer_offset_rw)
  LOC_VEC3_FIELD(gps_antenna) LOC_VEC3_FIELD(rear_axle)
  LOC_SCALAR_FIELD(wheel_radius) LOC_SCALAR_FIELD(gear_ratio) LOC_SCALAR_FIELD(wheelbase)
  LOC_SCALAR_FIELD(gps_pos_sigma_floor) LOC_SCALAR_FIELD(gps_vel_sigma_floor)
  LOC_SCALAR_FIELD(wheel_speed_sigma) LOC_SCALAR_FIELD(nhc_lateral_sigma)
  LOC_SCALAR_FIELD(nhc_vertical_sigma) LOC_SCALAR_FIELD(steering_yaw_rate_sigma)
  LOC_SCALAR_FIELD(mag_sigma) LOC_SCALAR_FIELD(expected_accel_sigma)
  LOC_VEC3_FIELD(mag_hard_iron) LOC_SCALAR_FIELD(mag_declination_deg)
  LOC_SCALAR_FIELD(mag_inclination_deg)
  LOC_SCALAR_FIELD(align_time_s) LOC_SCALAR_FIELD(align_max_gyro)
  LOC_SCALAR_FIELD(align_max_accel_dev) LOC_SCALAR_FIELD(heading_init_min_speed)
  LOC_SCALAR_FIELD(heading_init_min_dist) LOC_SCALAR_FIELD(init_sigma_vel)
  LOC_SCALAR_FIELD(init_sigma_tilt) LOC_SCALAR_FIELD(init_sigma_yaw)
  LOC_SCALAR_FIELD(init_sigma_yaw_mag) LOC_SCALAR_FIELD(init_sigma_accel_bias)
  LOC_SCALAR_FIELD(init_sigma_gyro_bias) LOC_SCALAR_FIELD(init_sigma_wheel_scale)
  LOC_SCALAR_FIELD(init_sigma_steer_offset) LOC_SCALAR_FIELD(max_consecutive_rejects)
  LOC_SCALAR_FIELD(heading_init_max_yaw_rate) LOC_SCALAR_FIELD(max_reject_time_s)
  LOC_SCALAR_FIELD(max_pos_rms) LOC_SCALAR_FIELD(ready_time_s) LOC_SCALAR_FIELD(max_imu_gap_s)
  LOC_SCALAR_FIELD(fusion_delay_s)
#undef LOC_SCALAR_FIELD
#undef LOC_VEC3_FIELD
  return false;
}

[[noreturn]] void usage() {
  std::fprintf(stderr,
               "usage: replay -o PREFIX [--sensors imu,gps,gpsvel,rpm,steer,mag,accel|gpsinit]\n"
               "              [--datum lat,lon,alt] [--est-every N] [--gps-latency S]\n"
               "              [--set name=value ...] LOG.csv [LOG.csv ...]\n");
  std::exit(2);
}

// Corrections and whether this build has them (config.hpp LOC_USE_*).
constexpr std::pair<const char*, bool> kCompiled[] = {
    {"gps", LOC_USE_GPS_POS}, {"gpsvel", LOC_USE_GPS_VEL}, {"rpm", LOC_USE_WHEEL_SPEED},
    {"steer", LOC_USE_STEERING}, {"mag", LOC_USE_MAG}, {"accel", LOC_USE_EXPECTED_ACCEL}};

}  // namespace

int main(int argc, char** argv) {
  std::string prefix;
  std::set<std::string> sensors = {"imu"};
  for (const auto& [name, on] : kCompiled) {
    if (on) sensors.insert(name);
  }
  std::vector<double> datum;
  int est_every = 4;
  double gps_latency = 0;
  loc::Config cfg;
  std::vector<const char*> files;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) {
      prefix = argv[++i];
    } else if (a == "--sensors" && i + 1 < argc) {
      const auto s = split(argv[++i], ',');
      sensors = {s.begin(), s.end()};
    } else if (a == "--datum" && i + 1 < argc) {
      for (const auto& s : split(argv[++i], ',')) datum.push_back(std::strtod(s.c_str(), nullptr));
    } else if (a == "--gps-latency" && i + 1 < argc) {
      gps_latency = std::strtod(argv[++i], nullptr);
    } else if (a == "--est-every" && i + 1 < argc) {
      est_every = std::max(1, std::atoi(argv[++i]));
    } else if (a == "--set" && i + 1 < argc) {
      if (!set_config(cfg, argv[++i])) {
        std::fprintf(stderr, "unknown or malformed --set %s\n", argv[i]);
        return 2;
      }
    } else if (a.starts_with("-")) {
      usage();
    } else {
      files.push_back(argv[i]);
    }
  }
  if (prefix.empty() || files.empty() || (!datum.empty() && datum.size() != 3)) usage();
  // Never silently ignore a requested sensor: that makes comparisons lie.
  for (const auto& [name, on] : kCompiled) {
    if (!on && sensors.contains(name)) {
      std::fprintf(stderr, "'%s' is compiled out of this build (config.hpp); use replay_all\n",
                   name);
      return 2;
    }
  }

  std::vector<Event> events;
  for (const char* f : files) {
    if (!load(f, events)) {
      std::fprintf(stderr, "cannot read %s\n", f);
      return 1;
    }
  }
  for (Event& e : events) {
    if (e.kind == Kind::Gps) e.arrival = e.t + gps_latency;
  }
  // Arrival order; at equal times the IMU goes first so corrections see the latest state.
  std::ranges::stable_sort(events, [](const Event& a, const Event& b) {
    if (a.arrival != b.arrival) return a.arrival < b.arrival;
    return (a.kind == Kind::Imu) > (b.kind == Kind::Imu);
  });

  FILE* est = std::fopen((prefix + "_est.csv").c_str(), "w");
  FILE* inn = std::fopen((prefix + "_innov.csv").c_str(), "w");
  if (!est || !inn) {
    std::fprintf(stderr, "cannot write %s_*.csv\n", prefix.c_str());
    return 1;
  }
  std::fprintf(est,
               "t,ready,faults,e,n,u,ve,vn,vu,roll,pitch,yaw,bax,bay,baz,bgx,bgy,bgz,lat,lon,alt,"
               "sig_e,sig_n,sig_u,rms_h");
  for (const char* n : loc::kSensorNames) std::fprintf(est, ",nis_%s", n);
  std::fprintf(est, "\n");
  std::fprintf(inn, "t,sensor,dim,nis,gate,accepted,y0,y1,y2\n");

  loc::LiveLocalizer L(cfg);
  if (!datum.empty()) L.set_datum(datum[0], datum[1], datum[2]);
  const bool gps_init_only = sensors.contains("gpsinit");
  const auto use = [&](const char* s) { return sensors.contains(s); };
  const auto status = [&] { return L.filter().status(); };

  using Clock = std::chrono::steady_clock;
  Clock::duration imu_time{}, total_time{};
  long n_imu_running = 0, n_imu = 0;
  std::uint32_t last_faults = 0;
  double t_init = -1;

  for (const Event& e : events) {
    const auto t0 = Clock::now();
    const auto& v = e.v;
    switch (e.kind) {
      case Kind::Imu: {
        if (v.size() < 6) break;
        const bool running = status() == loc::Status::Running;
        L.on_imu(e.t, loc::Vec3(v[0], v[1], v[2]), loc::Vec3(v[3], v[4], v[5]));
        if (running) {
          imu_time += Clock::now() - t0;
          ++n_imu_running;
        }
        break;
      }
      case Kind::Gps: {
        const bool want = use("gps") || (gps_init_only && status() != loc::Status::Running);
        if (!want || v.size() < 5) break;
        loc::GpsFix fix;
        fix.t = e.t;
        fix.lat_deg = v[0];
        fix.lon_deg = v[1];
        fix.alt_m = v[2];
        fix.h_acc = loc::Scalar(v[3]);
        fix.v_acc = loc::Scalar(v[4]);
        if (v.size() >= 9 && (use("gpsvel") || gps_init_only)) {
          fix.has_vel = true;
          fix.vel_enu = loc::Vec3(v[5], v[6], v[7]);
          fix.s_acc = loc::Scalar(v[8]);
        }
        L.on_gps(fix);
        break;
      }
      case Kind::Rpm:
        if (use("rpm")) L.on_wheel_rpm(e.t, loc::Scalar(v[0]));
        break;
      case Kind::Steer:
        if (use("steer")) L.on_steering(e.t, loc::Scalar(v[0]));
        break;
      case Kind::Mag:
        if (use("mag") && v.size() >= 3) L.on_mag(e.t, loc::Vec3(v[0], v[1], v[2]));
        break;
      case Kind::Accel:
        if (use("accel")) L.on_expected_accel(e.t, loc::Scalar(v[0]));
        break;
    }
    total_time += Clock::now() - t0;

    for (const loc::Innovation& in : L.innovations()) {
      std::fprintf(inn, "%.6f,%s,%d,%.4g,%.4g,%d,%.5g,%.5g,%.5g\n", in.t,
                   loc::kSensorNames[static_cast<int>(in.sensor)], in.dim, double(in.nis),
                   double(in.gate), in.accepted ? 1 : 0, double(in.y[0]), double(in.y[1]),
                   double(in.y[2]));
    }
    L.clear_innovations();

    if (status() == loc::Status::Running && t_init < 0) {
      t_init = e.t;
      std::printf("initialised at t=%.3f s\n", e.t);
    }
    // "Raise error": report each new fault. A vehicle would stop here.
    if (L.faults() != last_faults) {
      std::printf("FAULT at t=%.3f s: 0x%x%s%s%s%s\n", e.t, L.faults(),
                  (L.faults() & loc::kFaultInnovation) ? " innovation" : "",
                  (L.faults() & loc::kFaultPositionRms) ? " position_rms" : "",
                  (L.faults() & loc::kFaultImuGap) ? " imu_gap" : "",
                  (L.faults() & loc::kFaultNumerical) ? " numerical" : "");
      last_faults = L.faults();
    }

    if (e.kind == Kind::Imu && status() == loc::Status::Running && (n_imu++ % est_every) == 0) {
      const loc::Output o = L.output();
      std::fprintf(est,
                   "%.6f,%d,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.6f,%.6f,%.6f,%.5g,%.5g,%.5g,%.5g,"
                   "%.5g,%.5g,%.10f,%.10f,%.4f,%.5g,%.5g,%.5g,%.5g",
                   o.t, o.ready ? 1 : 0, o.faults, double(o.p_enu.x()), double(o.p_enu.y()), double(o.p_enu.z()),
                   double(o.v_enu.x()), double(o.v_enu.y()), double(o.v_enu.z()),
                   double(o.rpy.x()), double(o.rpy.y()), double(o.rpy.z()),
                   double(o.accel_bias.x()), double(o.accel_bias.y()), double(o.accel_bias.z()),
                   double(o.gyro_bias.x()), double(o.gyro_bias.y()), double(o.gyro_bias.z()),
                   o.lat_deg, o.lon_deg, o.alt_m, double(o.pos_sigma.x()),
                   double(o.pos_sigma.y()), double(o.pos_sigma.z()), double(o.pos_rms_h));
      for (const auto r : o.nis_ratio) std::fprintf(est, ",%.4g", double(r));
      std::fprintf(est, "\n");
    }
  }
  std::fclose(est);
  std::fclose(inn);

  const auto us = [](Clock::duration d) {
    return std::chrono::duration<double, std::micro>(d).count();
  };
  const loc::Output o = L.output();
  std::printf("events=%zu  final faults=0x%x  late measurements=%u\n", events.size(), o.faults,
              o.late_measurements);
  if (n_imu_running > 0) {
    std::printf("host cost: %.2f us per IMU step, %.1f ms total\n", us(imu_time) / n_imu_running,
                us(total_time) / 1000);
  }
  return t_init < 0 ? 3 : 0;
}
