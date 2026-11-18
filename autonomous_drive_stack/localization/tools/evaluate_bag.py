#!/usr/bin/env python3
"""Evaluate the filter on an Isaac Sim rosbag2 recording (from localization/, after building).

  python3 -I tools/evaluate_bag.py BAG.db3 --name mila_20261114

Converts the bag (tools/rosbag_to_replay.py) with synthetic RTK GPS at 8 Hz and
1 Hz, replays IMU-only / IMU+GPS / late GPS / +mag, scores against
/ground_truth/pose, and writes results/<name>.md and results/<name>_error.png.
"""
import argparse
import importlib.util
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def load_module(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).parent / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


ev = load_module("evaluate")
rb = load_module("rosbag_to_replay")

# The recordings start already moving, so level over 1 s without the per-sample
# stillness check, and take heading from GPS once above 0.5 m/s.
INIT = ["align_time_s=1", "align_max_accel_dev=5", "heading_init_min_speed=0.5",
        "init_sigma_tilt=0.04"]


def mag_reference(bag):
    """Sim's magnetic field direction in the world frame, from ground-truth attitude."""
    d = rb.read_bag(bag)
    mag, gt = d["/mag"], d["/ground_truth/pose"]
    m = mag[(mag[:, 0] >= gt[0, 0]) & (mag[:, 0] <= gt[-1, 0])]
    _, q = rb.interp_pose(gt, m[:, 0])
    e, n, u = np.einsum("nij,nj->ni", rb.quat_to_R(q), m[:, 1:4]).mean(0)
    return np.degrees(np.arctan2(e, n)), np.degrees(np.arctan2(-u, np.hypot(e, n)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bag")
    ap.add_argument("--name", required=True)
    args = ap.parse_args()
    for d in ("data", "out", "results"):
        Path(d).mkdir(exist_ok=True)
    decl, incl = mag_reference(args.bag)
    variants = [  # name, binary, sensors, extra args, label
        ("imu_only", "replay", "imu,gpsinit", [], "IMU only (GPS just to initialise)"),
        ("imu_gps", "replay", "imu,gps,gpsvel", [], "IMU + GPS"),
        ("imu_gps_late", "replay", "imu,gps,gpsvel", ["--gps-latency", "0.08"],
         "IMU + GPS, GPS arriving 80 ms late"),
        # Magnetometer is compiled out of the car build, so use replay_all. Its reference
        # field comes from ground truth, so this row is optimistic.
        ("imu_gps_mag", "replay_all", "imu,gps,gpsvel,mag",
         ["--set", f"mag_declination_deg={decl:.2f}", "--set", f"mag_inclination_deg={incl:.2f}"],
         "IMU + GPS + magnetometer (field from ground truth: optimistic)"),
    ]
    lines = [f"# {args.name}: Isaac Sim recording", "",
             f"Bag: `{Path(args.bag).name}`. GPS is **synthetic** (the recording has none): "
             "ground truth + RTK-like noise (~1 cm), antenna 0.8 m above the IMU. Ground-truth pose "
             "is assumed to be the IMU position. Overrides for a moving start: "
             f"`{' '.join(INIT)}`. Magnetometer reference inferred from ground truth: "
             f"declination {decl:.1f} deg, inclination {incl:.1f} deg.", ""]
    traces = {}
    for hz in (8, 1):
        ds = f"{args.name}_{hz}hz"
        info = subprocess.run(["python3", "-I", "tools/rosbag_to_replay.py", args.bag, "--out",
                               f"data/{ds}", "--gps-rate", str(hz)],
                              check=True, capture_output=True, text=True).stdout.strip()
        truth = ev.load(f"data/{ds}_truth.csv")
        if hz == 8:
            lines += [f"Recording: {info}.", ""]
        lines += [f"## Synthetic RTK at {hz} Hz", "",
                  "| variant | RMS | 95% | max | max (ready) | believed RMS | honesty | > 2x | ready % | faults |",
                  "|---|---|---|---|---|---|---|---|---|---|"]
        at_fix, th, held = ev.gps_only(ds, truth)
        s = ev.stats(held)
        lines.append(f"| GPS only, held between fixes | {ev.fmt_m(s['rms'])} | {ev.fmt_m(s['p95'])} | "
                     f"{ev.fmt_m(s['max'])} | - | - | - | - | - | - |")
        traces[(hz, "gps_held")] = (th, held)
        for name, binary, sensors, extra, label in variants:
            cmd = [str(ev.BIN / binary), "-o", f"out/{ds}_{name}", "--datum", ev.DATUM,
                   "--est-every", "1", "--sensors", sensors, *extra]
            for kv in INIT:
                cmd += ["--set", kv]
            stdout = ev.run(cmd + [f"data/{ds}_sensors.csv", f"data/{ds}_gps.csv"])
            est = ev.load(f"out/{ds}_{name}_est.csv")
            est = est[(est["t"] >= truth["t"][0]) & (est["t"] <= truth["t"][-1])]
            err = np.hypot(est["e"] - np.interp(est["t"], truth["t"], truth["e"]),
                           est["n"] - np.interp(est["t"], truth["t"], truth["n"]))
            s = ev.stats(err)
            believed = np.sqrt(np.mean(est["rms_h"] ** 2))
            honesty, beyond = ev.consistency(err, est["rms_h"])
            faults = [l.split("FAULT at ")[1] for l in stdout.splitlines() if l.startswith("FAULT")]
            lines.append(f"| {label} | {ev.fmt_m(s['rms'])} | {ev.fmt_m(s['p95'])} | "
                         f"{ev.fmt_m(s['max'])} | {ev.max_while_ready(err, est)} | "
                         f"{ev.fmt_m(believed)} | {honesty:.2f} | "
                         f"{beyond:.0f}% | {100 * np.mean(est['ready']):.0f} | "
                         f"{'; '.join(faults) or 'none'} |")
            traces[(hz, name)] = (est["t"], err)
        lines.append("")
    Path(f"results/{args.name}.md").write_text("\n".join(lines))
    print("\n".join(lines))
    plot(traces, args.name)


def plot(traces, name):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.6), sharey=True, facecolor=ev.SURFACE)
    for ax, hz in zip(axes, (8, 1)):
        ev.style(ax)
        ends = []
        for (key, label), color in zip([("imu_only", "IMU only"), ("gps_held", "GPS only (held)"),
                                        ("imu_gps", "IMU + GPS")], ev.SERIES):
            t, err = traces[(hz, key)]
            r = ev.rolling_rms(t, err)
            ax.plot(t, np.maximum(r, 1e-4), color=color, linewidth=1.4, label=label,
                    zorder=4 if key == "imu_gps" else 3)
            ends.append([max(r[-30:].max(), 1e-3), label])
        ends.sort()
        for i in range(1, len(ends)):
            ends[i][0] = max(ends[i][0], ends[i - 1][0] * 1.9)
        x_end = max(traces[(hz, "imu_gps")][0])
        for y, label in ends:
            ax.text(x_end + 0.3, y, f" {label}", color=ev.INK2, fontsize=8, va="center")
        ev.ref_lines(ax, 0.3)
        ax.set_yscale("log")
        ax.set_ylim(1e-3, 1e2)
        ax.set_xlim(0, x_end + 4)
        ax.set_title(f"Synthetic RTK at {hz} Hz", color=ev.INK, fontsize=11, loc="left")
        ax.set_xlabel("sim time (s)", color=ev.INK2, fontsize=9)
    axes[0].set_ylabel("horizontal error, 1 s RMS (m, log)", color=ev.INK2, fontsize=9)
    axes[0].legend(loc="upper left", fontsize=8, frameon=False, labelcolor=ev.INK2)
    fig.suptitle(f"Isaac Sim recording {name}: IMU only vs GPS only vs fused", color=ev.INK,
                 fontsize=12, x=0.01, ha="left")
    fig.tight_layout()
    fig.savefig(f"results/{name}_error.png", dpi=150, facecolor=ev.SURFACE)


if __name__ == "__main__":
    main()
