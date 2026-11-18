#!/usr/bin/env python3
"""Synthetic car drive with ground truth, in the replay CSV format.

Stand-in until Isaac Sim / real logs exist. A kinematic-bicycle car drives
S-bends, a long turn and gentle hills; every sensor is sampled from the truth
with datasheet-style noise (ISM330DHCX, u-blox RTK, inverter rpm, etc.).

  python3 tools/sim_drive.py --out data/nominal --gps-rate 8
  python3 tools/sim_drive.py --out data/stress --gps-rate 8 --stress

Writes <out>_sensors.csv, <out>_gps.csv, <out>_truth.csv. Geodetic conversion
uses GeographicLib's CartConvert (same library the filter uses).

--stress adds: a 10 s GPS outage (60-70 s), RTK float (90-100 s), one bad fix
(110 s) and three bad fixes in a row (120 s, should raise an innovation fault).

--realistic adds errors the filter does NOT model, so results aren't best-case:
GPS time stamps 2 ms off and not on IMU samples, antenna and rear axle 2-3 cm
from where config.hpp says, IMU mounted 0.5 deg crooked, rear tyres that slip
sideways in turns and spin when accelerating, more vibration than the filter
assumes, and a 4 cm multipath jump (30-40 s) the receiver doesn't flag.
"""
import argparse
import subprocess

import numpy as np

DATUM = (35.3000, -120.6625, 100.0)  # Cal Poly-ish; ellipsoidal height
# Must match include/loc/config.hpp
GPS_ANTENNA = np.array([0.0, 0.0, 0.8])
REAR_AXLE = np.array([-0.8, 0.0, -0.2])
WHEEL_RADIUS, GEAR_RATIO, WHEELBASE = 0.23, 4.0, 1.55
MAG_DECL, MAG_INCL, MAG_FIELD_UT = np.radians(11.6), np.radians(59.0), 47.0

# Sensor truth-model parameters (what the "real" sensors do)
ACCEL_DENSITY = 1.0e-3  # m/s^2/sqrt(Hz): datasheet ~7e-4 + vibration
GYRO_DENSITY = 1.0e-4   # rad/s/sqrt(Hz): datasheet 8.7e-5 + vibration
ACCEL_BIAS0, GYRO_BIAS0 = 0.05, np.radians(0.3)  # 1-sigma turn-on bias
ACCEL_BIAS_RW, GYRO_BIAS_RW = 5e-5, 5e-6
ACCEL_LSB, GYRO_LSB = 0.122e-3 * 9.80665, np.radians(17.5e-3)  # +-4 g, +-500 dps
EARTH_RATE = 7.2921150e-5


def smoothstep(t, t0, width):
    x = np.clip((t - t0) / width, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def normal_gravity(lat_deg, h):
    """WGS84 Somigliana + free-air; matches GeographicLib::NormalGravity to ~1e-5."""
    s2 = np.sin(np.radians(lat_deg)) ** 2
    return 9.7803253359 * (1 + 0.00193185265241 * s2) / np.sqrt(1 - 0.00669437999013 * s2) - 3.086e-6 * h


def skew_log(Ra, Rb):
    """Rotation vectors of Ra^T Rb, vectorised over the first axis."""
    D = np.einsum("nji,njk->nik", Ra, Rb)
    cos = np.clip((np.trace(D, axis1=1, axis2=2) - 1) / 2, -1, 1)
    ang = np.arccos(cos)
    v = np.stack([D[:, 2, 1] - D[:, 1, 2], D[:, 0, 2] - D[:, 2, 0], D[:, 1, 0] - D[:, 0, 1]], axis=1)
    scale = np.where(ang < 1e-8, 0.5, ang / (2 * np.sin(np.maximum(ang, 1e-12))))
    return v * scale[:, None]


def skew_exp(w):
    """Rotation matrix for rotation vector w (Rodrigues)."""
    a = np.linalg.norm(w)
    if a < 1e-12:
        return np.eye(3)
    k = w / a
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + np.sin(a) * K + (1 - np.cos(a)) * K @ K


def rpy_zyx(R):
    return np.stack([np.arctan2(R[:, 2, 1], R[:, 2, 2]), np.arcsin(-R[:, 2, 0]),
                     np.arctan2(R[:, 1, 0], R[:, 0, 0])], axis=1)


def enu_to_lla(enu):
    out = subprocess.run(["CartConvert", "-r", "-p", "6", "-l", *map(str, DATUM)],
                         input="\n".join(f"{e:.6f} {n:.6f} {u:.6f}" for e, n, u in enu),
                         capture_output=True, text=True, check=True).stdout
    return np.array([list(map(float, line.split())) for line in out.strip().splitlines()])


def simulate_truth(duration, imu_rate, rear_axle, slip_per_mps2=0.0, sub=8):
    """Fine-grid truth: time, IMU position/velocity/accel, attitude, rear-axle speed, steering.

    slip_per_mps2: rear-tyre sideslip angle (rad) per m/s^2 of lateral acceleration.
    """
    dt = 1.0 / (imu_rate * sub)
    t = np.arange(0, duration + dt / 2, dt)
    # Horizontal speed (m/s): stationary 5 s, then a mix of speeds, stop at the end.
    v = 0.0
    for t0, dv in [(5, 6), (20, 4), (45, -6), (60, 8), (85, -5), (105, 3), (128, -7), (140, -3)]:
        v = v + dv * smoothstep(t, t0, 4.0)
    v = np.maximum(v, 0.0)
    # Desired lateral accel -> steering angle (S-bends, then one long left turn).
    a_lat = (3.5 * np.sin(2 * np.pi * (t - 8) / 18) * smoothstep(t, 8, 3) * (1 - smoothstep(t, 58, 3))
             + 2.5 * smoothstep(t, 62, 3) * (1 - smoothstep(t, 80, 3))
             + 3.0 * np.sin(2 * np.pi * (t - 90) / 12) * smoothstep(t, 90, 3) * (1 - smoothstep(t, 135, 3)))
    steer = np.arctan(a_lat * WHEELBASE / np.maximum(v, 3.0) ** 2)
    yaw_rate = v * np.tan(steer) / WHEELBASE

    def cumtrapz(y):
        return np.concatenate([[0.0], np.cumsum((y[1:] + y[:-1]) / 2 * dt)])

    yaw = np.radians(30.0) + cumtrapz(yaw_rate)
    s = cumtrapz(v)
    grade = 0.8 * 2 * np.pi / 150 * np.cos(2 * np.pi * s / 150) + 0.3 * 2 * np.pi / 47 * np.cos(2 * np.pi * s / 47)
    rear = np.stack([cumtrapz(v * np.cos(yaw)), cumtrapz(v * np.sin(yaw)),
                     0.8 * np.sin(2 * np.pi * s / 150) + 0.3 * np.sin(2 * np.pi * s / 47)], axis=1)

    # Attitude: body x along the 3-D path (turned into the turn by any tyre slip),
    # small roll outward in turns.
    heading = yaw + slip_per_mps2 * a_lat
    xb = np.stack([np.cos(heading), np.sin(heading), grade], axis=1)
    xb /= np.linalg.norm(xb, axis=1, keepdims=True)
    yb0 = np.cross([0.0, 0.0, 1.0], xb)
    yb0 /= np.linalg.norm(yb0, axis=1, keepdims=True)
    zb0 = np.cross(xb, yb0)
    roll = np.radians(0.5) * a_lat
    yb = np.cos(roll)[:, None] * yb0 + np.sin(roll)[:, None] * zb0
    zb = -np.sin(roll)[:, None] * yb0 + np.cos(roll)[:, None] * zb0
    R = np.stack([xb, yb, zb], axis=2)  # columns = body axes in ENU (body -> world)

    p_imu = rear - R @ rear_axle
    v_imu = np.gradient(p_imu, dt, axis=0, edge_order=2)
    a_imu = np.gradient(v_imu, dt, axis=0, edge_order=2)
    speed3d = v * np.sqrt(1 + grade**2)
    return dict(t=t, dt=dt, sub=sub, R=R, p=p_imu, v=v_imu, a=a_imu, speed=speed3d,
                steer=steer, accel_long=np.gradient(speed3d, dt))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--duration", type=float, default=150.0)
    ap.add_argument("--imu-rate", type=float, default=416.0, help="ISM330DHCX ODR closest to 450 Hz")
    ap.add_argument("--gps-rate", type=float, default=8.0)
    ap.add_argument("--stress", action="store_true")
    ap.add_argument("--realistic", action="store_true")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)

    # Truth for the --realistic errors; zero/identity otherwise.
    real = args.realistic
    antenna = GPS_ANTENNA + ([0.03, -0.02, 0.02] if real else 0)  # true mounting points
    rear_axle = REAR_AXLE + ([0.03, 0.0, 0.02] if real else 0)
    gps_clock_error = 0.002 if real else 0.0     # GPS stamps are this late, s
    gps_phase = 0.0013 if real else 0.0          # fixes fall between IMU samples
    tilt = np.radians([0.3, -0.5, 0.2]) if real else np.zeros(3)  # IMU mounting error
    R_body_imu = skew_exp(tilt)
    accel_density = 3e-3 if real else ACCEL_DENSITY  # vibration (filter assumes 1.6e-3)
    gyro_density = 2.5e-4 if real else GYRO_DENSITY  # (filter assumes 1.7e-4)

    tr = simulate_truth(args.duration, args.imu_rate, rear_axle, 0.004 if real else 0.0)
    t, R, sub = tr["t"], tr["R"], tr["sub"]
    g_w = np.array([0.0, 0.0, -normal_gravity(DATUM[0], DATUM[2])])
    lat = np.radians(DATUM[0])
    w_ie = EARTH_RATE * np.array([0.0, np.cos(lat), np.sin(lat)])

    # ---------------- IMU: sample k covers the interval (t[k-1], t[k]] ----------------
    idx = np.arange(0, len(t), sub)
    t_imu = t[idx]
    n = len(idx)
    # specific force in body frame (incl. Coriolis), averaged over each interval
    f_w = tr["a"] - g_w + 2 * np.cross(w_ie, tr["v"])
    f_b = np.einsum("nji,nj->ni", R, f_w)
    f_avg = np.zeros((n, 3))
    for j in range(sub + 1):
        wgt = 0.5 if j in (0, sub) else 1.0
        f_avg[1:] += wgt * f_b[idx[:-1] + j]
    f_avg[1:] /= sub
    f_avg[0] = f_b[0]
    w_avg = np.zeros((n, 3))
    w_avg[1:] = skew_log(R[idx[:-1]], R[idx[1:]]) * args.imu_rate
    w_avg += np.einsum("nji,j->ni", R[idx], w_ie)  # Earth rotation

    dt_imu = 1.0 / args.imu_rate
    ba = rng.normal(0, ACCEL_BIAS0, 3) + np.cumsum(rng.normal(0, ACCEL_BIAS_RW * np.sqrt(dt_imu), (n, 3)), axis=0)
    bg = rng.normal(0, GYRO_BIAS0, 3) + np.cumsum(rng.normal(0, GYRO_BIAS_RW * np.sqrt(dt_imu), (n, 3)), axis=0)
    f_avg, w_avg = f_avg @ R_body_imu, w_avg @ R_body_imu  # body -> (crooked) IMU axes
    acc = f_avg + ba + rng.normal(0, accel_density * np.sqrt(args.imu_rate), (n, 3))
    gyr = w_avg + bg + rng.normal(0, gyro_density * np.sqrt(args.imu_rate), (n, 3))
    acc = np.round(acc / ACCEL_LSB) * ACCEL_LSB
    gyr = np.round(gyr / GYRO_LSB) * GYRO_LSB

    def at(times):  # nearest fine-grid index
        return np.clip(np.round(times / tr["dt"]).astype(int), 0, len(t) - 1)

    # ---------------- GPS / RTK ----------------
    t_gps = np.arange(0, args.duration, 1.0 / args.gps_rate) + gps_phase  # time stamps
    k = at(t_gps - gps_clock_error)                                       # true times
    p_ant = tr["p"][k] + R[k] @ antenna
    v_ant = np.gradient(tr["p"] + R @ antenna, tr["dt"], axis=0)[k]
    # RTK fixed: white noise + slowly-varying (multipath) Gauss-Markov error, ~1 cm total
    sig_w, sig_gm, tau = np.array([0.007, 0.007, 0.012]), np.array([0.007, 0.007, 0.010]), 30.0
    phi = np.exp(-1.0 / args.gps_rate / tau)
    gm = np.zeros((len(k), 3))
    gm[0] = rng.normal(0, sig_gm)
    for i in range(1, len(k)):
        gm[i] = phi * gm[i - 1] + rng.normal(0, sig_gm * np.sqrt(1 - phi**2))
    p_meas = p_ant + gm + rng.normal(0, sig_w, (len(k), 3))
    if real:
        p_meas[(t_gps >= 30) & (t_gps < 40)] += [0.04, 0.0, 0.0]  # unflagged multipath jump
    v_meas = v_ant + rng.normal(0, 0.02, (len(k), 3))
    h_acc, v_acc, s_acc = np.full(len(k), 0.014), np.full(len(k), 0.02), np.full(len(k), 0.03)
    keep = np.ones(len(k), bool)
    if args.stress:
        keep &= ~((t_gps >= 60) & (t_gps < 70))                 # outage
        flt = (t_gps >= 90) & (t_gps < 100)                     # RTK float
        p_meas[flt] += rng.normal(0, [0.25, 0.25, 0.4], (flt.sum(), 3))
        h_acc[flt], v_acc[flt] = 0.30, 0.45
        for t_bad, count in [(110.0, 1), (120.0, 3)]:           # wrong fixes, claimed RTK
            i0 = np.searchsorted(t_gps, t_bad)
            p_meas[i0:i0 + count] += [1.5, -1.0, 0.0]
    lla = enu_to_lla(p_meas)

    # ---------------- vehicle sensors at 100 Hz, expected accel at 50 Hz ----------------
    t_veh = np.arange(0, args.duration, 0.01)
    kv = at(t_veh)
    spin = 1 + (0.01 * np.maximum(tr["accel_long"][kv], 0) if real else 0)  # 1% per m/s^2
    rpm = (tr["speed"][kv] * 1.005 * spin / WHEEL_RADIUS * GEAR_RATIO * 60 / (2 * np.pi)
           + rng.normal(0, 3.0, len(kv)))
    steer = tr["steer"][kv] + np.radians(0.1) + rng.normal(0, np.radians(0.3), len(kv))
    m_world = MAG_FIELD_UT * np.array([np.cos(MAG_INCL) * np.sin(MAG_DECL),
                                       np.cos(MAG_INCL) * np.cos(MAG_DECL), -np.sin(MAG_INCL)])
    motor_disturbance = 3.0 * np.abs(tr["accel_long"][kv])[:, None] * np.array([0.6, 0.3, -0.7])
    mag = (np.einsum("nji,j->ni", R[kv], m_world) + np.array([2.0, -1.5, 1.0])
           + motor_disturbance + rng.normal(0, 0.3, (len(kv), 3)))
    t_cmd = np.arange(0, args.duration, 0.02)
    a_true = tr["accel_long"][at(t_cmd)]
    a_lag = np.zeros_like(a_true)
    for i in range(1, len(a_lag)):  # actuator lag ~0.15 s
        a_lag[i] = a_lag[i - 1] + (a_true[i] - a_lag[i - 1]) * 0.02 / 0.15
    moving = tr["speed"][at(t_cmd)] > 0.5
    a_cmd = 1.1 * a_lag - 0.2 * moving + rng.normal(0, 0.3, len(a_lag))

    # ---------------- write ----------------
    hdr = f"# synthetic drive, datum {DATUM[0]},{DATUM[1]},{DATUM[2]}, seed {args.seed}"
    with open(f"{args.out}_sensors.csv", "w") as f:
        f.write(hdr + "\n")
        np.savetxt(f, np.column_stack([t_imu, acc, gyr]), fmt="imu,%.6f" + ",%.6f" * 6)
        np.savetxt(f, np.column_stack([t_veh, rpm]), fmt="rpm,%.6f,%.2f")
        np.savetxt(f, np.column_stack([t_veh, steer]), fmt="steer,%.6f,%.6f")
        np.savetxt(f, np.column_stack([t_veh, mag]), fmt="mag,%.6f,%.3f,%.3f,%.3f")
        np.savetxt(f, np.column_stack([t_cmd, a_cmd]), fmt="accel,%.6f,%.4f")
    with open(f"{args.out}_gps.csv", "w") as f:
        f.write(hdr + "\n")
        cols = np.column_stack([t_gps, lla, h_acc, v_acc, v_meas, s_acc])[keep]
        np.savetxt(f, cols, fmt="gps,%.6f,%.11f,%.11f,%.4f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f")
    with open(f"{args.out}_truth.csv", "w") as f:
        p_ant_imu = tr["p"][idx] + R[idx] @ antenna
        np.savetxt(f, np.column_stack([t_imu, tr["p"][idx], tr["v"][idx], rpy_zyx(R[idx]), p_ant_imu,
                                       ba, bg]),
                   fmt="%.6f" + ",%.5f" * 6 + ",%.6f" * 3 + ",%.5f" * 3 + ",%.5f" * 6,
                   header="t,e,n,u,ve,vn,vu,roll,pitch,yaw,ant_e,ant_n,ant_u,bax,bay,baz,bgx,bgy,bgz",
                   comments="")
    print(f"wrote {args.out}_*.csv: {n} IMU samples, {keep.sum()} GPS fixes, "
          f"path {np.sum(np.linalg.norm(np.diff(tr['p'], axis=0), axis=1)):.0f} m, "
          f"max speed {tr['speed'].max():.1f} m/s")


if __name__ == "__main__":
    main()
