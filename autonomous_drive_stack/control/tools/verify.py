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
    veh = KinematicBicycle(VehicleParams())
    pth = P.circle(radius=R, turns=1.0)
    cfg = PurePursuitConfig()
    log = run(pth, controller=PurePursuit(pth, cfg), vehicle=veh,
              v_target=6.0, t_max=90.0)
    tail = log.arr("steer")[int(len(log.t) * 0.3):]
    measured, expected = float(np.mean(tail)), analytic_steer_for_radius(veh.p.wheel_base, R)
    err = abs(measured - expected)
    print(f"  {R:>7.1f} {math.degrees(expected):>9.3f}d {math.degrees(measured):>9.3f}d "
          f"{math.degrees(err):>7.3f}d {log.xte_rms:>8.3f}m")
    check(f"circle R={R:g} matches analytic", err < math.radians(0.6),
          f"off by {math.degrees(err):.3f} deg")


print("\n=== 3. LOOKAHEAD SWEEP on a slalom (behaviour, not pass/fail) ===")
print(f"  {'ld_min':>7} {'xte rms':>9} {'xte max':>9} {'signed mean':>12} {'reversals':>10}")
pth = P.slalom(length=160.0, amplitude=4.0, wavelength=40.0)
for ld in (2.0, 3.0, 5.0, 9.0, 15.0):
    cfg = PurePursuitConfig(k_lookahead=0.0, ld_min=ld, ld_max=ld)
    log = run(pth, controller=PurePursuit(pth, cfg), v_target=6.0, t_max=40.0)
    print(f"  {ld:>7.1f} {log.xte_rms:>8.3f}m {log.xte_max:>8.3f}m "
          f"{log.xte_mean_signed:>+11.3f}m {log.steer_reversals:>10d}")
print("  -> small ld: low error, more hunting. large ld: smoother, cuts corners.")


print("\n=== 4. FIGURE-OF-EIGHT (self-intersecting path) ===")
# Feasibility FIRST. At radius 12 the lobes need R=2.51 m and the car can only
# manage 2.77 m, so no controller can track it — the reference is the problem.
_p = VehicleParams()
for r in (20.0, 30.0):
    ok, msg = P.figure_eight(radius=r).check_feasible(_p.wheel_base, _p.max_steer)
    print(f"  radius {r:g}: {msg}")
pth = P.figure_eight(radius=30.0)
ok, _ = pth.check_feasible(_p.wheel_base, _p.max_steer)
check("reference is physically trackable", ok, "min radius vs vehicle limit")
log = run(pth, v_target=5.0, t_max=120.0)
check("does not jump branches at the crossing", log.xte_max < 1.5,
      f"max |xte| = {log.xte_max:.3f} m")
check("finished the path", len(log.t) > 100, f"{len(log.t)} steps, {log.t[-1]:.1f} s")


print("\n=== 5. ACKERMANN SPLIT (inner wheel must turn more than outer) ===")
veh = KinematicBicycle(VehicleParams())
for d in (5.0, 15.0, 25.0):
    li, lo = veh.ackermann_wheel_angles(math.radians(d))
    check(f"inner > outer at {d:g} deg", li > lo,
          f"inner {math.degrees(li):.2f} deg, outer {math.degrees(lo):.2f} deg")


print("\n=== 6. MPC vs PURE PURSUIT (the reason for the extra complexity) ===")
# Pure Pursuit's corner-cutting is structural: a steady SIGNED bias that grows
# with curvature and cannot be tuned away. MPC sees the path's curvature over
# the whole horizon, so it can start turning before the corner arrives. If MPC
# does not reduce the signed bias, it is not earning its keep.
try:
    from mila_control.mpc import MPC, MPCConfig
    import time as _time
    print(f"  {'path':<16}{'PP signed':>11}{'MPC signed':>12}{'improvement':>13}")
    for _name, _p in (("circle R=15", P.circle(radius=15.0, turns=1.0)),
                      ("slalom 4/40", P.slalom(160.0, 4.0, 40.0)),
                      ("figure8 R=30", P.figure_eight(radius=30.0))):
        _pp = run(_p, controller=PurePursuit(_p, PurePursuitConfig()), v_target=6.0, t_max=200.0)
        _mp = run(_p, controller=MPC(_p, MPCConfig()), v_target=6.0, t_max=200.0)
        _a, _b = abs(_pp.xte_mean_signed), abs(_mp.xte_mean_signed)
        _r = _a / _b if _b > 1e-9 else float('inf')
        print(f"  {_name:<16}{_pp.xte_mean_signed:>+10.3f}m{_mp.xte_mean_signed:>+11.3f}m{_r:>12.1f}x")
        if _name == "circle R=15":
            check("MPC cuts the corner-cutting bias", _b < _a / 5.0,
                  f"{_a:.3f} m -> {_b:.3f} m")

    # Real-time feasibility. The control period at 20 Hz is 50 ms; if the solve
    # does not fit inside it with margin, MPC cannot run on the vehicle at all.
    _p = P.slalom(160.0, 4.0, 40.0)
    _c = MPC(_p, MPCConfig())
    _v = KinematicBicycle(VehicleParams())
    from mila_control.vehicle import VehicleState as _VS, speed_controller as _sc
    _v.state = _VS(v=6.0)
    _t = []
    for _i in range(200):
        _s = _v.state
        _t0 = _time.perf_counter()
        _st, _dbg = _c.step(_s.x, _s.y, _s.yaw, _s.v, 0.05)
        _t.append((_time.perf_counter() - _t0) * 1e3)
        _v.step(_st, _sc(_s.v, 6.0), 0.05)
    _t = np.array(_t[5:])
    # Threshold is 35 ms, not something tighter: the budget is 50 ms and solve
    # time varies with machine and load, so a tight bound here would flake
    # rather than inform. What this asserts is "comfortably inside the period",
    # which is the property that actually matters. Expect ~10-20 ms on a laptop;
    # budget 2-3x that on the Orin's ARM cores and re-measure there.
    check("MPC solves inside the 50 ms control period",
          float(np.percentile(_t, 95)) < 35.0,
          f"mean {_t.mean():.1f} ms, p95 {np.percentile(_t,95):.1f} ms, budget 50 ms")

    # MPC should also land near the analytic steady state, but NOT exactly: the
    # r_steer term regularises the command slightly toward zero, so a small bias
    # is correct behaviour rather than an error. Tolerance reflects that.
    _p = P.circle(radius=15.0, turns=1.0)
    _lg = run(_p, controller=MPC(_p, MPCConfig()), v_target=6.0, t_max=200.0)
    _meas = float(np.mean(_lg.arr("steer")[int(len(_lg.t) * 0.4):]))
    _exp = analytic_steer_for_radius(VehicleParams().wheel_base, 15.0)
    check("MPC steady state near analytic", abs(_meas - _exp) < math.radians(0.3),
          f"off by {math.degrees(abs(_meas - _exp)):.4f} deg (regularised, so not 0)")
except ImportError as _e:
    print(f"  SKIPPED: {_e}")


print("\n" + "=" * 62)
if FAIL:
    print(f"{len(FAIL)} CHECK(S) FAILED: " + ", ".join(FAIL))
    sys.exit(1)
print("all checks passed")
