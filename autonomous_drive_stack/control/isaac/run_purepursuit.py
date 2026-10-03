#!/usr/bin/env python3
"""
OCTOBER 9 DELIVERABLE — Pure Pursuit closed-loop in Isaac Sim.

This file is deliberately thin. All the control logic lives in `mila_control`,
which has no Isaac imports at all and is tested by `tools/verify.py` against
known analytic answers. What is left here is only the adapter:

    read pose from the articulation  ->  PurePursuit.step()  ->  write joint targets

Two things this adapter is responsible for, and they are where sim integrations
usually go wrong:

1.  THE POSE MUST BE AT THE REAR AXLE. Isaac reports the articulation root pose,
    which is generally at or near the vehicle's centre. Pure Pursuit's geometry
    is derived about the rear axle. Feeding a centre pose straight in gives a
    persistent cross-track bias that reads exactly like a tuning problem — so
    the conversion is explicit below, via `--pose-at-cg`.

2.  THE QUATERNION ORDER. Isaac returns (w, x, y, z); scipy and ROS use
    (x, y, z, w). Swap them and yaw looks nearly right near zero and diverges as
    the car turns.

Run with Isaac's bundled interpreter:

    ./python.sh .../control/isaac/run_purepursuit.py --path slalom
"""

from __future__ import annotations

import argparse
import math
import os
import sys

parser = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--headless", action="store_true")
parser.add_argument("--vehicle", choices=["leatherback", "mila"],
                    default="leatherback")
parser.add_argument("--list-joints", action="store_true")
parser.add_argument("--path", choices=["straight", "circle", "slalom", "figure_eight"],
                    default="slalom")
parser.add_argument("--speed", type=float, default=4.0, help="m/s target")
parser.add_argument("--k-lookahead", type=float, default=0.35, help="ld = k*v, s")
parser.add_argument("--ld-min", type=float, default=1.5)
parser.add_argument("--ld-max", type=float, default=10.0)
parser.add_argument("--pose-at-cg", action="store_true",
                    help="articulation root is at the CG, not the rear axle; "
                         "shift the pose back by half a wheelbase before control")
parser.add_argument("--duration-s", type=float, default=120.0)
parser.add_argument("--physics-dt", type=float, default=1.0 / 60.0)
parser.add_argument("--csv", type=str, default=None,
                    help="write the run log here for offline comparison against "
                         "the same path run on the bicycle model")
args = parser.parse_args()

from isaacsim import SimulationApp                                  # noqa: E402

sim_app = SimulationApp({"headless": args.headless})

import numpy as np                                                   # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import stage as mila_stage                                          # noqa: E402
from mila_control import path as P                                  # noqa: E402
from mila_control.pure_pursuit import PurePursuit, PurePursuitConfig  # noqa: E402
from mila_control.vehicle import VehicleParams, speed_controller    # noqa: E402

STEER_JOINTS = ["Knuckle__Upright__Front_Left", "Knuckle__Upright__Front_Right"]
DRIVE_JOINTS = ["Wheel__Upright__Rear_Left", "Wheel__Upright__Rear_Right"]

PATHS = {
    "straight": lambda: P.straight(80.0),
    "circle": lambda: P.circle(radius=15.0, turns=1.5),
    "slalom": lambda: P.slalom(length=80.0, amplitude=4.0, wavelength=20.0),
    "figure_eight": lambda: P.figure_eight(radius=20.0),
}


def main() -> int:
    p = VehicleParams()
    path = PATHS[args.path]()

    # Feasibility BEFORE driving. If the reference demands a tighter radius than
    # the steering can produce, no controller can track it and the resulting
    # divergence is not a controller bug. Checking first saves hours of tuning
    # something that was never trackable. (This is how the figure-eight at
    # radius 12 was diagnosed: it needs R=2.51 m, the car can do 2.77 m.)
    feasible, msg = path.check_feasible(p.wheel_base, p.max_steer)
    print(f"\npath '{args.path}': {path.length:.1f} m, {msg}")
    if not feasible:
        print("REFUSING TO RUN: the path is not physically trackable by this "
              "vehicle. Widen the path or raise max_steer.")
        return 1

    cfg = PurePursuitConfig(wheel_base=p.wheel_base,
                            k_lookahead=args.k_lookahead,
                            ld_min=args.ld_min, ld_max=args.ld_max,
                            max_steer=p.max_steer)
    controller = PurePursuit(path, cfg)

    world = mila_stage.make_world(args.physics_dt)
    vehicle = mila_stage.load_vehicle(world, args.vehicle)
    world.reset()

    idx = mila_stage.resolve_joints(vehicle, STEER_JOINTS, DRIVE_JOINTS,
                                    list_only=args.list_joints)
    if idx is None:
        return 0 if args.list_joints else 1
    steer_idx, drive_idx = idx

    from isaacsim.robot.wheeled_robots.controllers.ackermann_controller import (
        AckermannController)
    ackermann = AckermannController(name="ackermann", wheel_base=p.wheel_base,
                                    track_width=p.track_width,
                                    front_wheel_radius=p.wheel_radius)

    rows, errors = [], []
    n_steps = int(args.duration_s / args.physics_dt)
    prev_pos, speed = None, 0.0

    print(f"  {'t':>6} {'steer':>8} {'ld':>6} {'xte':>8} {'v':>6}")

    for i in range(n_steps):
        t = i * args.physics_dt
        pos, quat = vehicle.get_world_poses()
        x, y = float(pos[0][0]), float(pos[0][1])
        yaw = mila_stage.quat_to_yaw(quat[0])

        # Finite-difference the speed rather than trusting a joint rate: wheel
        # velocity times radius overstates ground speed whenever a wheel slips,
        # and the lookahead law would then inflate ld at exactly the wrong moment.
        if prev_pos is not None:
            speed = math.hypot(x - prev_pos[0], y - prev_pos[1]) / args.physics_dt
        prev_pos = (x, y)

        cx, cy = controller.rear_axle(x, y, yaw, from_cg=args.pose_at_cg)
        steer, dbg = controller.step(cx, cy, yaw, speed, args.physics_dt)

        if controller.state.finished:
            print(f"\nreached the end of the path at t={t:.1f} s")
            break

        v_cmd = max(0.0, min(args.speed,
                             speed + speed_controller(speed, args.speed) * args.physics_dt))
        cmd = ackermann.forward(np.array([steer, 0.0, v_cmd, 0.0, args.physics_dt]))
        vehicle.set_joint_position_targets(
            np.asarray(cmd.joint_positions)[None, :2], joint_indices=steer_idx)
        vehicle.set_joint_velocity_targets(
            np.asarray(cmd.joint_velocities)[None, :2], joint_indices=drive_idx)

        world.step(render=not args.headless)

        errors.append(dbg.cross_track)
        rows.append((t, x, y, yaw, speed, steer, dbg.lookahead, dbg.cross_track))
        if i % int(0.5 / args.physics_dt) == 0:
            print(f"  {t:>6.2f} {math.degrees(steer):>7.2f}d {dbg.lookahead:>6.2f} "
                  f"{dbg.cross_track:>+8.3f} {speed:>6.2f}")

    if errors:
        e = np.asarray(errors)
        print(f"\ncross-track error: rms {np.sqrt(np.mean(e**2)):.3f} m | "
              f"max {np.max(np.abs(e)):.3f} m | mean signed {np.mean(e):+.3f} m")
        print("A large SIGNED mean is corner-cutting or a wrong reference point\n"
              "(try --pose-at-cg). Near-zero signed mean with large rms is\n"
              "oscillation — lower k_lookahead or raise ld_min.")

    if args.csv and rows:
        with open(args.csv, "w") as fh:
            fh.write("t,x,y,yaw,v,steer,lookahead,xte\n")
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
