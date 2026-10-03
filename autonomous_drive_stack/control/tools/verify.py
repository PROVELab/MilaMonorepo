#!/usr/bin/env python3
"""
Known-answer checks for the Pure Pursuit controller.

Run this before trusting any plot. Every case has an answer known independently
of the code under test, which is the only kind of test worth having on a
controller.

    uv run tools/verify.py        # or: python tools/verify.py
"""

from __future__ import annotations

import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from mila_control import path as P
from mila_control.pure_pursuit import (PurePursuit, PurePursuitConfig,
                                       analytic_steer_for_radius)
from mila_control.sim import run
from mila_control.vehicle import KinematicBicycle, VehicleParams

FAIL = []


def check(name: str, ok: bool, detail: str) -> None:
    print(f"  [{'PASS' if ok else 'FAIL'}] {name:<34} {detail}")
    if not ok:
        FAIL.append(name)


print("\n=== 1. STRAIGHT LINE ===")
pth = P.straight(60.0)
log = run(pth, v_target=8.0, t_max=20.0)
check("stays on the line", log.xte_max < 0.02, f"max |xte| = {log.xte_max:.4f} m")
check("steering stays centred", np.max(np.abs(log.arr("steer"))) < math.radians(0.5),
      f"max |steer| = {math.degrees(np.max(np.abs(log.arr('steer')))):.3f} deg")

print("\n--- same line, started 3 m off to the side (approach transient) ---")
log = run(pth, v_target=8.0, t_max=25.0, start_offset=3.0)
settled = log.arr("cross_track")[int(len(log.t) * 0.5):]
check("converges back onto the path", float(np.max(np.abs(settled))) < 0.05,
      f"|xte| after settling = {float(np.max(np.abs(settled))):.4f} m")
check("no overshoot oscillation", log.steer_reversals < 12,
      f"{log.steer_reversals} steering reversals")


print("\n=== 2. CONSTANT-RADIUS CIRCLE vs ANALYTIC delta = atan(L/R) ===")
print(f"  {'R (m)':>7} {'analytic':>10} {'measured':>10} {'err':>8} {'xte rms':>9}")
for R in (10.0, 15.0, 25.0, 40.0):
    veh = KinematicBicycle(VehicleParams(wheel_base=1.6))
    pth = P.circle(radius=R, turns=1.0)
    cfg = PurePursuitConfig(wheel_base=1.6, k_lookahead=0.5, ld_min=2.0, ld_max=8.0)
    log = run(pth, controller=PurePursuit(pth, cfg), vehicle=veh,
              v_target=6.0, t_max=90.0)
    tail = log.arr("steer")[int(len(log.t) * 0.3):]
    measured, expected = float(np.mean(tail)), analytic_steer_for_radius(1.6, R)
    err = abs(measured - expected)
    print(f"  {R:>7.1f} {math.degrees(expected):>9.3f}d {math.degrees(measured):>9.3f}d "
          f"{math.degrees(err):>7.3f}d {log.xte_rms:>8.3f}m")
    check(f"circle R={R:g} matches analytic", err < math.radians(0.6),
          f"off by {math.degrees(err):.3f} deg")


print("\n=== 3. LOOKAHEAD SWEEP on a slalom (behaviour, not pass/fail) ===")
print(f"  {'ld_min':>7} {'xte rms':>9} {'xte max':>9} {'signed mean':>12} {'reversals':>10}")
pth = P.slalom(length=80.0, amplitude=4.0, wavelength=20.0)
for ld in (1.0, 2.0, 4.0, 8.0, 14.0):
    cfg = PurePursuitConfig(wheel_base=1.6, k_lookahead=0.0, ld_min=ld, ld_max=ld)
    log = run(pth, controller=PurePursuit(pth, cfg), v_target=6.0, t_max=40.0)
    print(f"  {ld:>7.1f} {log.xte_rms:>8.3f}m {log.xte_max:>8.3f}m "
          f"{log.xte_mean_signed:>+11.3f}m {log.steer_reversals:>10d}")
print("  -> small ld: low error, more hunting. large ld: smoother, cuts corners.")


print("\n=== 4. FIGURE-OF-EIGHT (self-intersecting path) ===")
# Feasibility FIRST. At radius 12 the lobes need R=2.51 m and the car can only
# manage 2.77 m, so no controller can track it — the reference is the problem.
for r in (12.0, 20.0):
    ok, msg = P.figure_eight(radius=r).check_feasible(1.6, math.radians(30))
    print(f"  radius {r:g}: {msg}")
pth = P.figure_eight(radius=20.0)
ok, _ = pth.check_feasible(1.6, math.radians(30))
check("reference is physically trackable", ok, "min radius vs vehicle limit")
log = run(pth, v_target=5.0, t_max=120.0)
check("does not jump branches at the crossing", log.xte_max < 1.5,
      f"max |xte| = {log.xte_max:.3f} m")
check("finished the path", len(log.t) > 100, f"{len(log.t)} steps, {log.t[-1]:.1f} s")


print("\n=== 5. ACKERMANN SPLIT (inner wheel must turn more than outer) ===")
veh = KinematicBicycle(VehicleParams(wheel_base=1.6, track_width=1.2))
for d in (5.0, 15.0, 25.0):
    li, lo = veh.ackermann_wheel_angles(math.radians(d))
    check(f"inner > outer at {d:g} deg", li > lo,
          f"inner {math.degrees(li):.2f} deg, outer {math.degrees(lo):.2f} deg")


print("\n" + "=" * 62)
if FAIL:
    print(f"{len(FAIL)} CHECK(S) FAILED: " + ", ".join(FAIL))
    sys.exit(1)
print("all checks passed")
