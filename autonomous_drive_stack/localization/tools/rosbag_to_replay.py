#!/usr/bin/env python3
"""Convert an Isaac Sim rosbag2 (.db3) into replay CSVs. No ROS install needed.

  python3 -I tools/rosbag_to_replay.py BAG.db3 --out data/run [--gps-rate 8]

Reads /imu_noisy (sensor_msgs/Imu), /mag (MagneticField), /ground_truth/pose
(PoseStamped) using header stamps (sim time). Writes <out>_sensors.csv,
<out>_truth.csv and, because the recording has no GPS, a SYNTHETIC
<out>_gps.csv made from ground truth: RTK-like noise (same model as
sim_drive.py), antenna at GPS_ANTENNA in the body frame, sim world x/y/z
treated as east/north/up at DATUM.
Assumes the ground-truth pose is the IMU position (lever arm unknown).
"""
import argparse
import sqlite3
import struct
import subprocess

import numpy as np

DATUM = (35.3000, -120.6625, 100.0)
GPS_ANTENNA = np.array([0.0, 0.0, 0.8])  # must match config.hpp


class Cdr:
    """Minimal little-endian CDR reader (alignment relative to the payload)."""

    def __init__(self, b):
        self.b, self.o = b, 4  # skip encapsulation header

    def _al(self, n):
        self.o += (-(self.o - 4)) % n

    def _get(self, fmt, size, n=1):
        self._al(size)
        v = struct.unpack_from(f"<{n}{fmt}", self.b, self.o)
        self.o += size * n
        return v

    def header(self):
        sec, = self._get("i", 4)
        nsec, = self._get("I", 4)
        n, = self._get("I", 4)
        frame = self.b[self.o:self.o + n - 1].decode()
        self.o += n
        return sec + nsec * 1e-9, frame

    def f64(self, n):
        return self._get("d", 8, n)


def read_bag(path):
    con = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    topics = {i: n for i, n in con.execute("select id, name from topics")}
    out = {"/imu_noisy": [], "/mag": [], "/ground_truth/pose": []}
    for tid, data in con.execute("select topic_id, data from messages order by timestamp"):
        name = topics[tid]
        if name not in out:
            continue
        r = Cdr(data)
        t, _ = r.header()
        if name == "/imu_noisy":
            r.f64(4), r.f64(9)
            w = r.f64(3)
            r.f64(9)
            a = r.f64(3)
            out[name].append((t, *a, *w))
        elif name == "/mag":
            out[name].append((t, *r.f64(3)))
        else:
            out[name].append((t, *r.f64(3), *r.f64(4)))  # x y z, qx qy qz qw
    return {k: np.array(v) for k, v in out.items()}


def quat_to_R(q):
    x, y, z, w = q.T
    return np.stack([
        np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], -1),
        np.stack([2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], -1),
        np.stack([2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)], -1)], -2)


def interp_pose(gt, t):
    """Linear position, normalised-lerp quaternion at times t."""
    p = np.column_stack([np.interp(t, gt[:, 0], gt[:, i]) for i in (1, 2, 3)])
    i = np.clip(np.searchsorted(gt[:, 0], t) - 1, 0, len(gt) - 2)
    a = ((t - gt[i, 0]) / (gt[i + 1, 0] - gt[i, 0]))[:, None]
    q0, q1 = gt[i, 4:8], gt[i + 1, 4:8]
    q1 = np.where((np.sum(q0 * q1, 1) < 0)[:, None], -q1, q1)
    q = (1 - a) * q0 + a * q1
    return p, q / np.linalg.norm(q, axis=1, keepdims=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bag")
    ap.add_argument("--out", required=True)
    ap.add_argument("--gps-rate", type=float, default=8.0)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)
    d = read_bag(args.bag)
    imu, mag, gt = d["/imu_noisy"], d["/mag"], d["/ground_truth/pose"]

    with open(f"{args.out}_sensors.csv", "w") as f:
        f.write(f"# from {args.bag}: /imu_noisy, /mag (sim time)\n")
        np.savetxt(f, imu, fmt="imu,%.6f" + ",%.7f" * 6)
        np.savetxt(f, mag, fmt="mag,%.6f,%.6e,%.6e,%.6e")

    # Truth at IMU times (inside the ground-truth span)
    t = imu[(imu[:, 0] >= gt[0, 0]) & (imu[:, 0] <= gt[-1, 0]), 0]
    p, q = interp_pose(gt, t)
    R = quat_to_R(q)
    v = np.gradient(p, t, axis=0)
    rpy = np.column_stack([np.arctan2(R[:, 2, 1], R[:, 2, 2]), np.arcsin(-R[:, 2, 0]),
                           np.arctan2(R[:, 1, 0], R[:, 0, 0])])
    ant = p + R @ GPS_ANTENNA
    nan = np.full((len(t), 6), np.nan)
    np.savetxt(f"{args.out}_truth.csv", np.column_stack([t, p, v, rpy, ant, nan]), delimiter=",",
               fmt="%.6f", comments="",
               header="t,e,n,u,ve,vn,vu,roll,pitch,yaw,ant_e,ant_n,ant_u,bax,bay,baz,bgx,bgy,bgz")

    # Synthetic RTK GPS from ground truth (the recording has none)
    tg = np.arange(gt[0, 0], gt[-1, 0], 1.0 / args.gps_rate)
    def antenna_at(times):
        p_, q_ = interp_pose(gt, times)
        return p_ + quat_to_R(q_) @ GPS_ANTENNA

    ant_g = antenna_at(tg)
    # Velocity from ground truth +-20 ms around each fix (not from the neighbouring
    # fixes: at 1 Hz those are 2 s apart and smear out every turn).
    t_lo, t_hi = np.maximum(tg - 0.02, gt[0, 0]), np.minimum(tg + 0.02, gt[-1, 0])
    vel = ((antenna_at(t_hi) - antenna_at(t_lo)) / (t_hi - t_lo)[:, None]
           + rng.normal(0, 0.02, ant_g.shape))
    sig_w, sig_gm, tau = np.array([0.007, 0.007, 0.012]), np.array([0.007, 0.007, 0.010]), 30.0
    phi = np.exp(-1.0 / args.gps_rate / tau)
    gm = np.zeros_like(ant_g)
    gm[0] = rng.normal(0, sig_gm)
    for i in range(1, len(gm)):
        gm[i] = phi * gm[i - 1] + rng.normal(0, sig_gm * np.sqrt(1 - phi**2))
    meas = ant_g + gm + rng.normal(0, sig_w, ant_g.shape)
    lla = subprocess.run(["CartConvert", "-r", "-p", "6", "-l", *map(str, DATUM)],
                         input="\n".join(f"{e:.6f} {n:.6f} {u:.6f}" for e, n, u in meas),
                         capture_output=True, text=True, check=True).stdout
    lla = np.array([list(map(float, line.split())) for line in lla.strip().splitlines()])
    n = len(tg)
    with open(f"{args.out}_gps.csv", "w") as f:
        f.write(f"# SYNTHETIC RTK from /ground_truth/pose, datum {DATUM}, seed {args.seed}\n")
        np.savetxt(f, np.column_stack([tg, lla, np.full(n, 0.014), np.full(n, 0.02), vel,
                                       np.full(n, 0.03)]),
                   fmt="gps,%.6f,%.11f,%.11f,%.4f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f")
    print(f"IMU {len(imu)} @ {1 / np.median(np.diff(imu[:, 0])):.0f} Hz, mag {len(mag)}, "
          f"truth {len(gt)} @ {1 / np.median(np.diff(gt[:, 0])):.0f} Hz, synthetic GPS {n} @ "
          f"{args.gps_rate:g} Hz, sim time {imu[0, 0]:.2f}-{imu[-1, 0]:.2f} s, "
          f"path {np.sum(np.linalg.norm(np.diff(gt[:, 1:4], axis=0), axis=1)):.1f} m")


if __name__ == "__main__":
    main()
