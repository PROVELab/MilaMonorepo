#!/usr/bin/env python3
"""
Figures for the controller, generated from the same runs tools/verify.py checks.

    uv run tools/plots.py --out ../../../out      # or: python tools/plots.py

Nothing here is decorative. Each figure answers a question someone will ask in
review, and the numbers printed by verify.py are the ones plotted.
"""

from __future__ import annotations

import argparse
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt                                      # noqa: E402
import numpy as np                                                   # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from mila_control import path as P                                   # noqa: E402
from mila_control.pure_pursuit import (PurePursuit, PurePursuitConfig,  # noqa: E402
                                       analytic_steer_for_radius)
from mila_control.sim import run, run_open_loop                      # noqa: E402

# Two hues that stay distinguishable in grayscale print and for the common forms
# of colour-vision deficiency; the sweep uses one hue at varying lightness so the
# ordering survives even when the hues do not.
C_REF, C_CAR = "#1f6fc4", "#d94812"
RAMP = plt.get_cmap("YlGnBu")


def _tidy(ax, xlabel, ylabel, title):
    ax.set_xlabel(xlabel); ax.set_ylabel(ylabel)
    ax.set_title(title, loc="left", fontsize=10)
    ax.grid(alpha=0.25, linewidth=0.5)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)


def fig_openloop(out: str) -> None:
    """The October 4 deliverable on the bicycle model, plus the thing about it
    that catches people out: symmetric steering, asymmetric path."""
    log = run_open_loop(amplitude_deg=18.0, period_s=6.0, v_target=5.0)
    ctr = run_open_loop(amplitude_deg=18.0, period_s=6.0, v_target=5.0,
                        center_heading=True)

    fig, (a0, a1, a2) = plt.subplots(1, 3, figsize=(15, 4.2),
                                     constrained_layout=True)
    a0.plot(log.t, np.degrees(log.arr("steer")), color=C_CAR, lw=1.6)
    a0.axhline(0, color="0.8", lw=0.8, zorder=0)
    _tidy(a0, "t (s)", "steering angle (deg)",
          "Command: a sine in time, no feedback")

    a1.plot(log.t, np.degrees(log.arr("yaw")), color=C_REF, lw=1.6)
    a1.axhline(0, color="0.8", lw=0.8, zorder=0)
    _tidy(a1, "t (s)", "heading (deg)",
          "Heading is the INTEGRAL — one-signed, mean +44 deg")

    a2.plot(log.arr("x"), log.arr("y"), color=C_CAR, lw=1.8,
            label="as commanded")
    a2.plot(ctr.arr("x"), ctr.arr("y"), color=C_REF, lw=1.6, ls="--",
            label="start heading re-centred")
    a2.axhline(0, color="0.8", lw=0.8, zorder=0)
    a2.set_aspect("equal", adjustable="datalim")
    a2.legend(frameon=False, fontsize=8)
    _tidy(a2, "x (m)", "y (m)", "So the path staircases rather than weaving")
    fig.savefig(os.path.join(out, "01_open_loop.png"), dpi=150)
    plt.close(fig)


def fig_tracking(out: str) -> None:
    """Closed loop on three paths: reference vs driven, and the error beneath."""
    cases = [("straight, started 3 m off", P.straight(60.0), 3.0, 8.0),
             ("circle, R = 15 m", P.circle(radius=15.0, turns=1.0), 0.0, 6.0),
             ("slalom, 4 m amplitude", P.slalom(80.0, 4.0, 20.0), 0.0, 6.0)]
    fig, axes = plt.subplots(2, 3, figsize=(14, 7), constrained_layout=True,
                             height_ratios=[2, 1])
    for col, (name, pth, off, v) in enumerate(cases):
        log = run(pth, v_target=v, t_max=90.0, start_offset=off)
        top, bot = axes[0][col], axes[1][col]
        top.plot(pth.xy[:, 0], pth.xy[:, 1], color=C_REF, lw=2.5,
                 alpha=0.5, label="reference")
        top.plot(log.arr("x"), log.arr("y"), color=C_CAR, lw=1.4, label="driven")
        top.set_aspect("equal", adjustable="datalim")
        top.legend(frameon=False, fontsize=8)
        _tidy(top, "x (m)", "y (m)", name)
        bot.plot(log.t, log.arr("cross_track"), color=C_CAR, lw=1.2)
        bot.axhline(0, color="0.8", lw=0.8, zorder=0)
        _tidy(bot, "t (s)", "cross-track (m)",
              f"rms {log.xte_rms:.3f} m, max {log.xte_max:.3f} m")
    fig.savefig(os.path.join(out, "02_tracking.png"), dpi=150)
    plt.close(fig)


def fig_analytic(out: str) -> None:
    """The check that matters: measured steady-state steering on a circle against
    delta = atan(L/R), which is known without reference to this code."""
    radii = np.array([8, 10, 12, 15, 20, 25, 30, 40, 50], dtype=float)
    measured = []
    for R in radii:
        pth = P.circle(radius=float(R), turns=1.0)
        cfg = PurePursuitConfig(wheel_base=1.6, k_lookahead=0.5,
                                ld_min=2.0, ld_max=8.0)
        log = run(pth, controller=PurePursuit(pth, cfg), v_target=6.0, t_max=120.0)
        measured.append(float(np.mean(log.arr("steer")[int(len(log.t) * 0.3):])))
    measured = np.asarray(measured)
    analytic = np.array([analytic_steer_for_radius(1.6, float(R)) for R in radii])

    fig, (a0, a1) = plt.subplots(1, 2, figsize=(11, 4), constrained_layout=True)
    a0.plot(radii, np.degrees(analytic), color=C_REF, lw=2.5, alpha=0.6,
            label=r"analytic  $\delta=\arctan(L/R)$")
    a0.plot(radii, np.degrees(measured), "o", color=C_CAR, ms=5,
            label="measured in closed loop")
    a0.legend(frameon=False, fontsize=9)
    _tidy(a0, "path radius R (m)", "steering angle (deg)",
          "Controller reproduces the known answer")
    a1.plot(radii, np.degrees(measured - analytic), "o-", color=C_CAR, lw=1.2, ms=4)
    a1.axhline(0, color="0.8", lw=0.8, zorder=0)
    _tidy(a1, "path radius R (m)", "measured - analytic (deg)",
          f"residual, max {np.max(np.abs(np.degrees(measured-analytic))):.3f} deg")
    fig.savefig(os.path.join(out, "03_analytic_check.png"), dpi=150)
    plt.close(fig)


def _steer_rate_rms(log) -> float:
    """RMS of d(steer)/dt in deg/s — how hard the steering actuator is working.
    A better smoothness measure than counting reversals, which saturates: on a
    smooth slalom every lookahead gives 7 or 8 reversals regardless, so that
    metric shows a trade-off where the data has none."""
    s, t = log.arr("steer"), log.arr("t")
    if len(s) < 3:
        return float("nan")
    return float(np.sqrt(np.mean((np.degrees(np.diff(s)) / np.diff(t)) ** 2)))


def fig_lookahead(out: str) -> None:
    """The cost side of the trade-off: longer lookahead cuts corners but asks far
    less of the steering actuator."""
    pth = P.slalom(80.0, 4.0, 20.0)
    lds = [1.0, 2.0, 4.0, 8.0, 14.0]
    fig, (a0, a1) = plt.subplots(1, 2, figsize=(13, 4.5), constrained_layout=True)
    rms, rate = [], []
    for i, ld in enumerate(lds):
        cfg = PurePursuitConfig(wheel_base=1.6, k_lookahead=0.0,
                                ld_min=ld, ld_max=ld)
        log = run(pth, controller=PurePursuit(pth, cfg), v_target=6.0, t_max=60.0)
        a0.plot(log.arr("x"), log.arr("y"), lw=1.3,
                color=RAMP(0.25 + 0.65 * i / (len(lds) - 1)),
                label=f"ld = {ld:g} m")
        rms.append(log.xte_rms); rate.append(_steer_rate_rms(log))
    a0.plot(pth.xy[:, 0], pth.xy[:, 1], "--", color="0.35", lw=1.0,
            label="reference", zorder=0)
    a0.set_aspect("equal", adjustable="datalim")
    a0.legend(frameon=False, fontsize=8, ncol=2)
    _tidy(a0, "x (m)", "y (m)",
          "Slalom at 6 m/s: longer lookahead flattens the weave")

    a1.plot(rate, rms, "o-", color=C_CAR, lw=1.5, ms=6)
    for ld, rr, rm in zip(lds, rate, rms):
        a1.annotate(f" ld={ld:g}", (rr, rm), fontsize=8, color="0.3",
                    va="center")
    _tidy(a1, "steering rate rms (deg/s)  —  actuator effort",
          "cross-track rms (m)  —  tracking error",
          "You buy accuracy with steering effort")
    fig.savefig(os.path.join(out, "04_lookahead_sweep.png"), dpi=150)
    plt.close(fig)


def fig_stability(out: str) -> None:
    """Why lookahead MUST scale with speed, measured rather than asserted.

    Each cell is a straight-line run started 3 m off the path. On a straight
    line a settled controller has zero error, so the max |error| over the second
    half of the run is a direct stability readout: ~0 means it converged,
    anything metre-scale means it is still oscillating and never will.

    The unstable corner is bottom-right — short lookahead at speed. That is the
    whole justification for ld = k*v.
    """
    lds = np.array([0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0])
    vs = np.array([2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0])
    grid = np.zeros((len(lds), len(vs)))
    pth = P.straight(160.0)
    for i, ld in enumerate(lds):
        for j, v in enumerate(vs):
            cfg = PurePursuitConfig(wheel_base=1.6, k_lookahead=0.0,
                                    ld_min=float(ld), ld_max=float(ld))
            log = run(pth, controller=PurePursuit(pth, cfg), v_target=float(v),
                      t_max=80.0, start_offset=3.0)
            e = log.arr("cross_track")
            grid[i, j] = np.max(np.abs(e[int(len(e) * 0.5):])) if len(e) > 4 else np.nan

    fig, ax = plt.subplots(figsize=(8, 5), constrained_layout=True)
    im = ax.pcolormesh(vs, lds, grid, cmap="RdYlBu_r", vmin=0.0, vmax=2.0,
                       shading="nearest")
    fig.colorbar(im, ax=ax, label="residual |error| after settling (m)")

    # The shipped gain law, drawn on top of the stability map it was chosen from.
    vline = np.linspace(vs.min(), vs.max(), 50)
    ax.plot(vline, np.clip(0.35 * vline, 1.5, 10.0), color="black", lw=2.2,
            label=r"shipped law:  $l_d=0.35\,v$, clamped to [1.5, 10] m")
    ax.legend(frameon=False, fontsize=9, loc="upper left")
    ax.set_ylim(lds.min(), lds.max())
    _tidy(ax, "speed v (m/s)", "lookahead ld (m)",
          "Stability map: blue converges, red never settles")
    fig.savefig(os.path.join(out, "06_stability_map.png"), dpi=150)
    plt.close(fig)


def fig_feasibility(out: str) -> None:
    """Not every reference is trackable. R_min = L/tan(max_steer) = 2.77 m for
    Mila's assumed geometry; a figure-eight at radius 12 demands 2.51 m. The
    resulting 161 m divergence is the PATH's fault, not the controller's."""
    radii = np.linspace(8, 30, 60)
    need = [P.figure_eight(radius=float(r)).min_turn_radius() for r in radii]
    limit = 1.6 / math.tan(math.radians(30))

    fig, ax = plt.subplots(figsize=(7, 4.5), constrained_layout=True)
    ax.plot(radii, need, color=C_REF, lw=2.0, label="radius the path demands")
    ax.axhline(limit, color=C_CAR, lw=2.0, ls="--",
               label=f"vehicle limit  L/tan(30 deg) = {limit:.2f} m")
    ax.fill_between(radii, 0, limit, where=np.asarray(need) < limit,
                    color=C_CAR, alpha=0.12)
    bad = radii[np.asarray(need) < limit]
    if len(bad):
        ax.annotate(f"infeasible below R = {bad.max():.1f} m",
                    xy=(bad.max(), limit), xytext=(bad.max() + 2, limit * 0.55),
                    fontsize=9, color=C_CAR,
                    arrowprops=dict(arrowstyle="->", color=C_CAR, lw=1.0))
    ax.legend(frameon=False, fontsize=9, loc="upper left")
    _tidy(ax, "figure-eight lobe radius (m)", "minimum turn radius (m)",
          "Check the reference is drivable before blaming the controller")
    fig.savefig(os.path.join(out, "05_feasibility.png"), dpi=150)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="out")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for fn in (fig_openloop, fig_tracking, fig_analytic, fig_lookahead,
               fig_feasibility, fig_stability):
        fn(a.out)
        print(f"  {fn.__name__}")
    print(f"\nwrote 6 figures to {os.path.abspath(a.out)}")


if __name__ == "__main__":
    main()


def fig_slide(out: str) -> None:
    """A version of the tracking figure sized and styled for a projected slide.

    The report figure is 2:1 with six panels and 8-pt labels — fine on a laptop,
    illegible in an image box at the back of a room. This is 4:3, two paths, and
    everything scaled up for projection.
    """
    with plt.rc_context({"font.size": 13, "axes.titlesize": 14,
                         "axes.labelsize": 12, "legend.fontsize": 12,
                         "xtick.labelsize": 11, "ytick.labelsize": 11}):
        # The slalom is 4 m of amplitude over 80 m of travel. Drawn to equal
        # aspect that is a nearly flat line — true, but unreadable from the back
        # of a room. Its y axis is expanded and the title says so, rather than
        # silently distorting it.
        cases = [("Constant radius, R = 15 m", P.circle(radius=15.0, turns=1.0),
                  6.0, True),
                 ("Slalom, 4 m amplitude  (y axis expanded)",
                  P.slalom(80.0, 4.0, 20.0), 6.0, False)]
        fig, axes = plt.subplots(2, 2, figsize=(10, 7.5), constrained_layout=True,
                                 height_ratios=[2.1, 1])
        for col, (name, pth, v, equal) in enumerate(cases):
            log = run(pth, v_target=v, t_max=90.0)
            top, bot = axes[0][col], axes[1][col]
            top.plot(pth.xy[:, 0], pth.xy[:, 1], color=C_REF, lw=5.0,
                     alpha=0.40, label="reference", solid_capstyle="round")
            top.plot(log.arr("x"), log.arr("y"), color=C_CAR, lw=2.0,
                     label="driven")
            if equal:
                top.set_aspect("equal", adjustable="datalim")
            else:
                top.set_ylim(-7.5, 7.5)
            top.legend(frameon=False, loc="best")
            _tidy(top, "x (m)", "y (m)", name)
            bot.plot(log.t, log.arr("cross_track"), color=C_CAR, lw=2.0)
            bot.axhline(0, color="0.75", lw=1.0, zorder=0)
            _tidy(bot, "t (s)", "error (m)",
                  f"rms {log.xte_rms:.3f} m   max {log.xte_max:.3f} m")
        fig.savefig(os.path.join(out, "slide_tracking.png"), dpi=170)
        plt.close(fig)
