#!/usr/bin/env python3
"""
Closed loop in Isaac Sim — Pure Pursuit or MPC. (Oct 9 / Oct 20 deliverables.)

Thin by design. All the control logic lives in `mila_control`, which has no
Isaac imports and is tested against known analytic answers by tools/verify.py.
What is left here is the adapter:

    read pose -> controller.step() -> write physxVehicleController:steer

Both controllers expose the same `.step(x, y, yaw, v, dt) -> (steer, debug)`
interface, so `--controller mpc` swaps one for the other with nothing else
changing. That is what makes a like-for-like comparison possible.

    ./python.sh .../isaac/run_purepursuit.py --path circle
    ./python.sh .../isaac/run_purepursuit.py --path circle --controller mpc

Run run_openloop.py FIRST. If the car will not move open loop, a controller
cannot help, and you will waste time debugging the wrong layer.
"""

from __future__ import annotations

import argparse
import math
import os
import sys

parser = argparse.ArgumentParser(
    description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--stage", default=None)
parser.add_argument("--headless", action="store_true")
parser.add_argument("--list-vehicle", action="store_true")
parser.add_argument("--controller", choices=["pure_pursuit", "mpc"],
                    default="pure_pursuit")
parser.add_argument("--path", choices=["straight", "circle", "slalom", "figure_eight"],
                    default="circle")
parser.add_argument("--speed", type=float, default=5.0, help="m/s target")
parser.add_argument("--duration-s", type=float, default=120.0)
parser.add_argument("--physics-dt", type=float, default=1.0 / 60.0)
parser.add_argument("--steer-sign", type=float, default=1.0)
parser.add_argument("--kp-throttle", type=float, default=0.15,
                    help="throttle per m/s of speed error")
parser.add_argument("--csv", default=None,
                    help="log here, to compare against the bicycle-model run")
args = parser.parse_args()

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import stage as S                                                  # noqa: E402

try:
    STAGE_PATH = S.find_stage(args.stage)
except FileNotFoundError as e:
    print(e)
    sys.exit(1)

from mila_control import path as P                                 # noqa: E402
from mila_control.vehicle import VehicleParams                     # noqa: E402

# Build the path and check feasibility BEFORE booting Isaac. If the reference
# demands a tighter turn than Mila can make, no controller can track it and the
# divergence is not a controller bug. Mila's R_min is 5.09 m.
VP = VehicleParams()
PATHS = {
    "straight": lambda: P.straight(100.0),
    "circle": lambda: P.circle(radius=15.0, turns=1.5),
    "slalom": lambda: P.slalom(160.0, 4.0, 40.0),
    "figure_eight": lambda: P.figure_eight(radius=30.0),
}
path = PATHS[args.path]()
feasible, msg = path.check_feasible(VP.wheel_base, VP.max_steer)
print(f"stage: {STAGE_PATH}")
print(f"path '{args.path}': {path.length:.1f} m, {msg}")
if not feasible:
    print("REFUSING TO RUN: this path is not physically trackable by Mila.")
    sys.exit(1)

from isaacsim import SimulationApp                                 # noqa: E402

sim_app = SimulationApp({"headless": args.headless})

import numpy as np                                                 # noqa: E402


def make_controller():
    if args.controller == "mpc":
        from mila_control.mpc import MPC, MPCConfig
        return MPC(path, MPCConfig(wheel_base=VP.wheel_base,
                                   cg_to_rear_axle=VP.cg_to_rear_axle,
                                   max_steer=VP.max_steer))
    from mila_control.pure_pursuit import PurePursuit, PurePursuitConfig
    return PurePursuit(path, PurePursuitConfig(wheel_base=VP.wheel_base,
                                               cg_to_rear_axle=VP.cg_to_rear_axle,
                                               max_steer=VP.max_steer))


def main() -> int:
    world = S.open_stage(STAGE_PATH, args.physics_dt)
    world.reset()

    prim = S.get_vehicle(list_only=args.list_vehicle)
    if prim is None:
        return 0 if args.list_vehicle else 1

    ctl = make_controller()
    rows, errors = [], []
    prev, speed = None, 0.0
    n = int(args.duration_s / args.physics_dt)

    print(f"\ncontroller: {args.controller}")
    print(f"  {'t':>6} {'steer':>8} {'xte':>8} {'v':>6}")

    for i in range(n):
        t = i * args.physics_dt
        x, y, yaw = S.get_pose()

        # Finite-difference the speed from position rather than trusting a wheel
        # rate: wheel speed times radius overstates ground speed whenever a
        # wheel slips, and MPC's A and B matrices both scale with v.
        if prev is not None:
            speed = math.hypot(x - prev[0], y - prev[1]) / args.physics_dt
        prev = (x, y)

        # The pose above is the rigid body's, i.e. the centre of mass. Both
        # controllers' geometry is derived about the REAR AXLE, which on Mila is
        # 1.251 m back — not wheel_base/2. from_cg=True does that conversion.
        cx, cy = ctl.rear_axle(x, y, yaw, from_cg=True)
        steer, dbg = ctl.step(cx, cy, yaw, speed, args.physics_dt)

        if getattr(ctl, "finished", False) or getattr(
                getattr(ctl, "state", None), "finished", False):
            print(f"\nreached the end of the path at t={t:.1f} s")
            break

        err = args.speed - speed
        throttle = max(0.0, min(1.0, args.kp_throttle * err))
        brake = max(0.0, min(1.0, -args.kp_throttle * err)) if err < -0.5 else 0.0
        S.drive(prim, steer, accelerator=throttle, brake=brake,
                steer_sign=args.steer_sign)

        world.step(render=not args.headless)

        xte = dbg.cross_track
        errors.append(xte)
        rows.append((t, x, y, yaw, speed, steer, xte))
        if i % int(0.5 / args.physics_dt) == 0:
            print(f"  {t:>6.2f} {math.degrees(steer):>7.2f}d {xte:>+8.3f} {speed:>6.2f}")

    if errors:
        e = np.asarray(errors)
        print(f"\ncross-track error: rms {np.sqrt(np.mean(e**2)):.3f} m | "
              f"max {np.max(np.abs(e)):.3f} m | mean signed {np.mean(e):+.3f} m")
        print("Large SIGNED mean = corner-cutting or a wrong reference point.\n"
              "Near-zero signed mean with large rms = oscillation.\n"
              "Mirror-image tracking = wrong steer sign; try --steer-sign -1.")

    if args.csv and rows:
        with open(args.csv, "w") as fh:
            fh.write("t,x,y,yaw,v,steer,xte\n")
            for r in rows:
                fh.write(",".join(f"{v:.6f}" for v in r) + "\n")
        print(f"wrote {args.csv} ({len(rows)} rows)")
    return 0


if __name__ == "__main__":
    code = 1
    try:
        code = main()
    finally:
        sim_app.close()
    sys.exit(code)
