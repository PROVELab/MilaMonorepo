#!/usr/bin/env python3
"""Generate synthetic drives, replay every sensor combination, compare to truth.

  python3 tools/evaluate.py          (from localization/, after building)

Writes results/summary.md and results/*.png. Answers:
  * just IMU vs just GPS vs fused: how far off is each?
  * does the filter know how wrong it is, moment by moment? (see consistency())
  * do the faults fire when they should?
"""
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

DATUM = "35.3,-120.6625,100"
# Replay programs: the stack's build (autonomous_drive_stack/build) or a standalone build here.
BIN = next((d for d in (Path("../build/localization"), Path("build")) if (d / "replay").exists()),
           Path("../build/localization"))
SENSORS_FUSED = "imu,gps,gpsvel,rpm,steer"  # what the car's build (config.hpp) uses
# replay_all has every correction compiled in, to compare the ones the car leaves out.
# The magnetometer gets the hard-iron calibration the sim used (a calibrated sensor).
VARIANTS = [  # name, binary, sensors, extra args, label
    ("imu_only", "replay", "imu,gpsinit", [], "IMU only (GPS just to initialise)"),
    ("imu_gps", "replay", "imu,gps,gpsvel", [], "IMU + GPS"),
    ("fused", "replay", SENSORS_FUSED, [], "Fused: IMU + GPS + rpm + steering (car build)"),
    ("fused_late_gps", "replay", SENSORS_FUSED, ["--gps-latency", "0.08"],
     "fused, GPS arriving 80 ms late"),
    ("fused_accel", "replay_all", SENSORS_FUSED + ",accel", [], "fused + expected accel"),
    ("fused_mag", "replay_all", SENSORS_FUSED + ",mag", ["--set", "mag_hard_iron=2,-1.5,1"],
     "fused + magnetometer (calibrated)"),
    ("fused_f32", "replay_f32", SENSORS_FUSED, [], "fused, float32 build (ESP32 precision)"),
]
DATASETS = [  # name, sim args, description
    ("nominal", ["--gps-rate", "8"], "8 Hz RTK, no faults"),
    ("nominal_1hz", ["--gps-rate", "1"], "1 Hz RTK, no faults"),
    ("stress", ["--gps-rate", "8", "--stress"],
     "8 Hz RTK; outage 60-70 s, RTK float 90-100 s, bad fix 110 s, 3 bad fixes 120 s"),
    ("realistic", ["--gps-rate", "8", "--realistic", "--seed", "2"],
     "8 Hz RTK plus errors the filter doesn't model (timing, mounting, tyre slip, "
     "vibration, multipath; see sim_drive.py)"),
]
TARGET, LIMIT = 0.04, 0.1524  # 4 cm goal, 6 in max
INK, INK2, MUTED, SURFACE = "#0b0b0b", "#52514e", "#898781", "#fcfcfb"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]


def run(cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def load(path):
    return np.genfromtxt(path, delimiter=",", names=True, dtype=None, encoding=None)


def gps_only(name, truth):
    """Raw fixes (antenna) vs true antenna position: at fix times and held at 100 Hz."""
    rows = [l.split(",") for l in Path(f"data/{name}_gps.csv").read_text().splitlines()
            if l.startswith("gps,")]
    t = np.array([float(r[1]) for r in rows])
    out =subprocess.run(["CartConvert", "-p", "6", "-l", *DATUM.split(",")],
                         input="\n".join(f"{r[2]} {r[3]} {r[4]}" for r in rows),
                         capture_output=True, text=True, check=True).stdout
    enu = np.array([list(map(float, l.split())) for l in out.strip().splitlines()])
    at_fix = np.hypot(enu[:, 0] - np.interp(t, truth["t"], truth["ant_e"]),
                      enu[:, 1] - np.interp(t, truth["t"], truth["ant_n"]))
    th = np.arange(t[0], truth["t"][-1], 0.01)
    k = np.searchsorted(t, th, side="right") - 1
    held = np.hypot(enu[k, 0] - np.interp(th, truth["t"], truth["ant_e"]),
                    enu[k, 1] - np.interp(th, truth["t"], truth["ant_n"]))
    return at_fix, th, held


def rolling_rms(t, err, window_s=1.0):
    n = max(1, int(round(window_s / np.median(np.diff(t)))))
    return np.sqrt(np.convolve(err**2, np.ones(n) / n, mode="same"))


def stats(err):
    return dict(rms=np.sqrt(np.mean(err**2)), p95=np.percentile(err, 95), max=err.max())


def max_while_ready(err, est):
    """Worst error while the filter said OK to drive (the car only drives then)."""
    r = est["ready"] == 1
    return fmt_m(err[r].max()) if r.any() else "-"


def consistency(err, believed):
    """Does the filter know how wrong it is, moment by moment?

    honesty: RMS of (actual error / believed RMS error) over time. 1 = honest,
      above 1 = overconfident (its RMS fault trips too late), below 1 = cautious.
    beyond: % of time the actual error is over 2x the believed one. An honest
      filter does that about 2% of the time (2-D Gaussian: exp(-4)).
    """
    r = err / believed
    return np.sqrt(np.mean(r**2)), 100 * np.mean(r > 2)


def fmt_m(x):
    return f"{x * 100:.1f} cm" if x < 1 else f"{x:.1f} m"


def style(ax):
    ax.set_facecolor(SURFACE)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(MUTED)
    ax.tick_params(colors=INK2, labelsize=9)
    ax.grid(True, which="major", color="#e4e3df", linewidth=0.6)
    ax.set_axisbelow(True)


def ref_lines(ax, x_end):
    for y, txt in [(TARGET, "4 cm goal"), (LIMIT, "6 in limit")]:
        ax.axhline(y, color=MUTED, linewidth=1, linestyle=(0, (4, 3)), zorder=2)
        ax.text(x_end, y * 1.08, txt, color=INK2, fontsize=8, va="bottom", zorder=5,
                bbox=dict(facecolor=SURFACE, edgecolor="none", pad=1))


def main():
    Path("data").mkdir(exist_ok=True)
    Path("out").mkdir(exist_ok=True)
    Path("results").mkdir(exist_ok=True)
    lines = ["# Localization evaluation (synthetic drives)", "",
             "Generated by `tools/evaluate.py`. Errors are horizontal (east/north) vs ground truth, "
             "from filter initialisation to the end of the 150 s, 1.1 km drive. "
             "**Believed** = the RMS error the filter reports for itself (`rms_h`). "
             "**Honesty** = RMS over time of actual error / believed error: about 1 is honest, "
             "above 1 means the filter is overconfident (dangerous: its 6 in RMS fault trips "
             "late), below 1 is cautious. **> 2x** = % of time the actual error was more than "
             "twice the believed one (honest: about 2%). **Max (ready)** = worst error while "
             "the filter said it was OK to drive. **Ready %** = share of time the filter "
             "said it was OK to drive (running, believed RMS under 6 in for 3 s, no faults). "
             "The output is the live one: the filter runs 0.15 s behind and is predicted "
             "forward to the newest IMU sample, as on the car.", ""]
    traces = {}
    for ds, sim_args, desc in DATASETS:
        run(["python3", "tools/sim_drive.py", "--out", f"data/{ds}", *sim_args])
        truth = load(f"data/{ds}_truth.csv")
        lines += [f"## {ds}: {desc}", "",
                  "| variant | RMS | 95% | max | max (ready) | believed RMS | honesty | > 2x | ready % | faults (first time) |",
                  "|---|---|---|---|---|---|---|---|---|---|"]
        at_fix, th, held = gps_only(ds, truth)
        s = stats(at_fix)
        lines.append(f"| GPS only, at fix times (antenna) | {fmt_m(s['rms'])} | {fmt_m(s['p95'])} | "
                     f"{fmt_m(s['max'])} | - | - | - | - | - | - |")
        s = stats(held)
        lines.append(f"| GPS only, held between fixes at 100 Hz | {fmt_m(s['rms'])} | "
                     f"{fmt_m(s['p95'])} | {fmt_m(s['max'])} | - | - | - | - | - | - |")
        traces[(ds, "gps_held")] = (th, held, None)
        for name, binary, sensors, extra, label in VARIANTS:
            stdout = run([str(BIN / binary), "-o", f"out/{ds}_{name}", "--datum", DATUM,
                          "--sensors", sensors, *extra,
                          f"data/{ds}_sensors.csv", f"data/{ds}_gps.csv"])
            est = load(f"out/{ds}_{name}_est.csv")
            err = np.hypot(est["e"] - np.interp(est["t"], truth["t"], truth["e"]),
                           est["n"] - np.interp(est["t"], truth["t"], truth["n"]))
            s = stats(err)
            believed = np.sqrt(np.mean(est["rms_h"] ** 2))
            honesty, beyond = consistency(err, est["rms_h"])
            faults = [l.split("FAULT at ")[1] for l in stdout.splitlines() if l.startswith("FAULT")]
            lines.append(f"| {label} | {fmt_m(s['rms'])} | {fmt_m(s['p95'])} | {fmt_m(s['max'])} | "
                         f"{max_while_ready(err, est)} | {fmt_m(believed)} | {honesty:.2f} | {beyond:.0f}% | "
                         f"{100 * np.mean(est['ready']):.0f} | {'; '.join(faults) if faults else 'none'} |")
            traces[(ds, name)] = (est["t"], err, est)
            if ds == "nominal" and name == "fused":
                cost = [l for l in stdout.splitlines() if l.startswith("host cost")]
                abs_check = check_absolute(est)
        lines.append("")

    inn = load("out/stress_fused_innov.csv")
    gps = inn[inn["sensor"] == "gps_pos"]
    lines += ["## Checks", "",
              f"* Absolute output: lat/lon/alt converted back to ENU matches the filter's ENU "
              f"to {abs_check * 1000:.3f} mm (max).",
              f"* Stress, fused: {np.sum(gps['accepted'] == 0)} of {len(gps)} GPS fixes rejected by "
              f"the innovation gate (chi-square 99.9%).",
              f"* {cost[0] if cost else ''} (double, this Mac; the float build is in the replay output).",
              ""]
    Path("results/summary.md").write_text("\n".join(lines))
    print("\n".join(lines))
    plot_nominal(traces)
    plot_stress(traces, inn)


def check_absolute(est):
    out = subprocess.run(["CartConvert", "-p", "6", "-l", *DATUM.split(",")],
                         input="\n".join(f"{a:.10f} {b:.10f} {c:.4f}"
                                         for a, b, c in zip(est["lat"], est["lon"], est["alt"])),
                         capture_output=True, text=True, check=True).stdout
    enu = np.array([list(map(float, l.split())) for l in out.strip().splitlines()])
    return np.max(np.abs(enu - np.column_stack([est["e"], est["n"], est["u"]])))


def plot_nominal(traces):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.6), sharey=True, facecolor=SURFACE)
    for ax, ds, title in [(axes[0], "nominal", "8 Hz RTK"), (axes[1], "nominal_1hz", "1 Hz RTK")]:
        style(ax)
        ends = []
        for (key, label), color in zip([("imu_only", "IMU only"), ("gps_held", "GPS only (held)"),
                                        ("imu_gps", "IMU + GPS"), ("fused", "Fused (car build)")],
                                       SERIES):
            t, err, _ = traces[(ds, key)]
            r = rolling_rms(t, err)
            ax.plot(t, np.maximum(r, 1e-4), color=color, linewidth=1.4, label=label,
                    zorder=4 if key == "fused" else 3)
            ends.append([max(r[-200:].max(), 1e-3), label])
        # end labels, nudged apart (log space) so they don't collide
        ends.sort()
        for i in range(1, len(ends)):
            ends[i][0] = max(ends[i][0], ends[i - 1][0] * 1.9)
        for y, label in ends:
            ax.text(153, y, f" {label}", color=INK2, fontsize=8, va="center")
        ref_lines(ax, 2)
        ax.set_yscale("log")
        ax.set_ylim(1e-3, 1e3)
        ax.set_xlim(0, 175)
        ax.set_title(title, color=INK, fontsize=11, loc="left")
        ax.set_xlabel("time (s)", color=INK2, fontsize=9)
    axes[0].set_ylabel("horizontal error, 1 s RMS (m, log)", color=INK2, fontsize=9)
    axes[0].legend(loc="upper left", fontsize=8, frameon=False, labelcolor=INK2)
    fig.suptitle("Horizontal position error: IMU only vs GPS only vs fused", color=INK,
                 fontsize=12, x=0.01, ha="left")
    fig.tight_layout()
    fig.savefig("results/nominal_error.png", dpi=150, facecolor=SURFACE)


def plot_stress(traces, inn):
    t, err, est = traces[("stress", "fused")]
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(12, 6.4), sharex=True, facecolor=SURFACE,
                                 gridspec_kw=dict(height_ratios=[3, 2]))
    for ax in (a1, a2):
        style(ax)
        for x0, x1, txt in [(60, 70, "GPS outage"), (90, 100, "RTK float")]:
            ax.axvspan(x0, x1, color="#f0efec", zorder=0)
            if ax is a1:
                ax.text((x0 + x1) / 2, 0.75, txt, color=INK2, fontsize=8, ha="center")
    a1.plot(t, np.maximum(err, 1e-4), color=SERIES[0], linewidth=1.4, label="actual error")
    a1.plot(t, est["rms_h"], color=SERIES[1], linewidth=1.4, label="believed RMS (rms_h)")
    a1.text(t[-1] + 1, err[-1], " actual", color=INK2, fontsize=8, va="center")
    a1.text(t[-1] + 1, est["rms_h"][-1] * 1.6, " believed", color=INK2, fontsize=8, va="center")
    ref_lines(a1, 2)
    a1.set_yscale("log")
    a1.set_ylim(1e-3, 1.2)
    a1.set_ylabel("horizontal error (m, log)", color=INK2, fontsize=9)
    a1.legend(loc="upper left", fontsize=8, frameon=False, labelcolor=INK2)
    a1.set_title("Stress run, fused filter: does it know how wrong it is?", color=INK,
                 fontsize=11, loc="left")
    gps = inn[inn["sensor"] == "gps_pos"]
    ok = gps["accepted"] == 1
    a2.scatter(gps["t"][ok], gps["nis"][ok], s=9, color=SERIES[0], label="accepted", zorder=3)
    a2.scatter(gps["t"][~ok], gps["nis"][~ok], s=24, color=SERIES[1], marker="D",
               label="rejected", zorder=4, edgecolors=SURFACE, linewidths=1)
    a2.annotate("1 bad fix: rejected,\nno fault", (110, gps["nis"][~ok][0]), (96, 1.2e2),
                color=INK2, fontsize=8, arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.8))
    a2.text(121, 2.5e2, "3 bad fixes in a row", color=INK2, fontsize=8)
    a2.axhline(gps["gate"][0], color=MUTED, linewidth=1, linestyle=(0, (4, 3)))
    a2.text(2, gps["gate"][0] * 1.3, "gate (chi-square 99.9%)", color=INK2, fontsize=8)
    faults = np.nonzero(np.diff(est["faults"]) != 0)[0]
    for i in faults:
        a2.axvline(est["t"][i + 1], color=INK2, linewidth=1)
        a2.text(est["t"][i + 1] + 0.8, 3e3, "innovation fault", color=INK2, fontsize=8)
    a2.set_yscale("log")
    a2.set_ylim(1e-2, 1e5)
    a2.set_ylabel("GPS position NIS (log)", color=INK2, fontsize=9)
    a2.set_xlabel("time (s)", color=INK2, fontsize=9)
    a2.set_xlim(0, 160)
    a2.legend(loc="upper left", fontsize=8, frameon=False, labelcolor=INK2)
    fig.tight_layout()
    fig.savefig("results/stress_honesty.png", dpi=150, facecolor=SURFACE)


if __name__ == "__main__":
    main()
