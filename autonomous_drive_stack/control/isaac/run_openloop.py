#!/usr/bin/env python3
"""
OCTOBER 4 DELIVERABLE — a car turning back and forth in Isaac Sim, no controller.

Steering is a sine wave in time. There is no path, no feedback and no error
signal: the command depends only on the clock. That is the point of the task.
What it proves is the plumbing — the stage loads, physics ticks, the steering
joints respond, the drive joints spin, and the sign conventions are what you
think they are. Everything on October 9 sits on top of this.

Minimum parameter set (the assignment asked for the minimum):

    wheel_base, track_width, wheel_radius   vehicle geometry
    amplitude, period                       the steering wave
    speed                                   constant forward wheel rate
    physics_dt                              integration step

Run it with Isaac's bundled interpreter, NOT a system python:

    ./python.sh .../control/isaac/run_openloop.py            # Linux
    python.bat .../control/isaac/run_openloop.py             # Windows

First time on a new asset, start here — it prints the joint names and exits:

    ./python.sh .../isaac/run_openloop.py --list-joints
"""

from __future__ import annotations

import argparse
import math
import os
import sys

# --- SimulationApp FIRST. ----------------------------------------------------
# Nothing from isaacsim.* may be imported before this object exists; it is what
# boots the Kit runtime those extensions live in. Import them earlier and you
# get a bare ModuleNotFoundError that says nothing about ordering. This is the
# number-one reason a first Isaac script fails.
parser = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--headless", action="store_true",
                    help="no window — for CI or a remote box over SSH")
parser.add_argument("--vehicle", choices=["leatherback", "mila"],
                    default="leatherback")
parser.add_argument("--list-joints", action="store_true",
                    help="print the asset's joint names and exit")
parser.add_argument("--amplitude-deg", type=float, default=18.0)
parser.add_argument("--period-s", type=float, default=6.0)
parser.add_argument("--speed", type=float, default=4.0, help="m/s")
parser.add_argument("--duration-s", type=float, default=30.0)
parser.add_argument("--physics-dt", type=float, default=1.0 / 60.0)
args = parser.parse_args()

from isaacsim import SimulationApp                                  # noqa: E402

sim_app = SimulationApp({"headless": args.headless})
# -----------------------------------------------------------------------------

import numpy as np                                                   # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import stage as mila_stage                                           # noqa: E402
from mila_control.vehicle import VehicleParams                       # noqa: E402

# Leatherback's joint names. Wrong names fail SILENTLY — the car just never
# steers — so resolve_joints() refuses to run rather than let you debug a
# controller that was fine. Use --list-joints on any other asset.
STEER_JOINTS = ["Knuckle__Upright__Front_Left", "Knuckle__Upright__Front_Right"]
DRIVE_JOINTS = ["Wheel__Upright__Rear_Left", "Wheel__Upright__Rear_Right"]


def main() -> int:
    p = VehicleParams()
    world = mila_stage.make_world(args.physics_dt)
    vehicle = mila_stage.load_vehicle(world, args.vehicle)

    # Joints are not queryable until physics has initialised the articulation.
    world.reset()

    idx = mila_stage.resolve_joints(vehicle, STEER_JOINTS, DRIVE_JOINTS,
                                    list_only=args.list_joints)
    if idx is None:
        return 0 if args.list_joints else 1
    steer_idx, drive_idx = idx

    from isaacsim.robot.wheeled_robots.controllers.ackermann_controller import (
        AckermannController)

    # Isaac's controller takes the single virtual steering angle and does the
    # inner/outer split itself — the same split mila_control.vehicle computes, so
    # the two models agree on what `steer` means.
    ackermann = AckermannController(name="ackermann",
                                    wheel_base=p.wheel_base,
                                    track_width=p.track_width,
                                    front_wheel_radius=p.wheel_radius)

    amp = math.radians(args.amplitude_deg)
    n_steps = int(args.duration_s / args.physics_dt)

    print(f"\nopen loop: +/-{args.amplitude_deg:g} deg, {args.period_s:g} s period, "
          f"{args.speed:g} m/s, {args.duration_s:g} s\n")
    print(f"  {'t':>6} {'steer':>8} {'x':>8} {'y':>8} {'yaw':>8}")

    for i in range(n_steps):
        t = i * args.physics_dt
        steer = amp * math.sin(2.0 * math.pi * t / args.period_s)

        cmd = ackermann.forward(np.array(
            [steer, 0.0, args.speed, 0.0, args.physics_dt]))
        vehicle.set_joint_position_targets(
            np.asarray(cmd.joint_positions)[None, :2], joint_indices=steer_idx)
        vehicle.set_joint_velocity_targets(
            np.asarray(cmd.joint_velocities)[None, :2], joint_indices=drive_idx)

        world.step(render=not args.headless)

        if i % int(0.5 / args.physics_dt) == 0:
            pos, quat = vehicle.get_world_poses()
            yaw = mila_stage.quat_to_yaw(quat[0])
            print(f"  {t:>6.2f} {math.degrees(steer):>7.2f}d "
                  f"{pos[0][0]:>8.2f} {pos[0][1]:>8.2f} "
                  f"{math.degrees(yaw):>7.1f}d")

    # Sanity check, because "it ran" is not "it worked": a car steered back and
    # forth should have swept yaw through roughly +/- the amplitude and ended up
    # somewhere other than the origin. Zero yaw travel means the steering joints
    # never actually moved, which looks identical to success in a screenshot.
    pos, quat = vehicle.get_world_poses()
    print(f"\nfinal pose: x={pos[0][0]:.2f} y={pos[0][1]:.2f} "
          f"yaw={math.degrees(mila_stage.quat_to_yaw(quat[0])):.1f} deg")
    print("expect: nonzero travel in x, y oscillating about the start line,\n"
          "        yaw swinging both ways. A dead-straight path means the\n"
          "        steering joints are not being driven.")
    return 0


if __name__ == "__main__":
    code = 1
    try:
        code = main()
    finally:
        sim_app.close()
    sys.exit(code)
