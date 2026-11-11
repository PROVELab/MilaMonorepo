#!/usr/bin/env python3
"""
Open loop — a car turning back and forth, no controller. (Oct 4 deliverable.)

Steering is a sine wave in time. No path, no feedback: the command depends only
on the clock. What it proves is the plumbing — the stage loads, physics ticks,
the vehicle responds to a steering command, and the sign convention is what you
think it is.

RUN THIS FIRST on a new machine. If this does not move the car, nothing else
will, and you will know the problem is the integration rather than a controller.

    ./python.sh .../isaac/run_openloop.py --list-vehicle   # inspect, don't drive
    ./python.sh .../isaac/run_openloop.py

If it cannot find the stage it will tell you exactly where to get it — the
drivable Mila is in PROVELab/autonomous-drive-stack, NOT in this monorepo.
"""

from __future__ import annotations

import argparse
import math
import os
import sys

parser = argparse.ArgumentParser(
    description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--stage", default=None,
                    help="path to parking_lot.usda (auto-detected if omitted)")
parser.add_argument("--headless", action="store_true")
parser.add_argument("--list-vehicle", action="store_true",
                    help="print the vehicle prim's physx attributes and exit")
parser.add_argument("--amplitude-deg", type=float, default=18.0)
parser.add_argument("--period-s", type=float, default=6.0)
parser.add_argument("--throttle", type=float, default=0.25,
                    help="accelerator, 0..1 (PhysX takes throttle, not m/s)")
parser.add_argument("--duration-s", type=float, default=30.0)
parser.add_argument("--physics-dt", type=float, default=1.0 / 60.0)
parser.add_argument("--steer-sign", type=float, default=1.0,
                    help="flip to -1 if the car steers the wrong way")
args = parser.parse_args()

# Resolve the stage BEFORE booting Isaac: a clear file-not-found beats a
# 30-second Kit startup followed by an obscure USD error.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stage as S                                                  # noqa: E402

try:
    STAGE_PATH = S.find_stage(args.stage)
except FileNotFoundError as e:
    print(e)
    sys.exit(1)
print(f"stage: {STAGE_PATH}")

from isaacsim import SimulationApp                                 # noqa: E402

sim_app = SimulationApp({"headless": args.headless})

import numpy as np                                                 # noqa: E402


def main() -> int:
    world = S.open_stage(STAGE_PATH, args.physics_dt)
    world.reset()                      # physics must initialise before querying

    prim = S.get_vehicle(list_only=args.list_vehicle)
    if prim is None:
        return 0 if args.list_vehicle else 1

    amp = math.radians(args.amplitude_deg)
    n = int(args.duration_s / args.physics_dt)
    x0, y0, _ = S.get_pose()

    print(f"\nopen loop: +/-{args.amplitude_deg:g} deg, {args.period_s:g} s period, "
          f"throttle {args.throttle:g}, {args.duration_s:g} s")
    print(f"  {'t':>6} {'steer_cmd':>10} {'norm':>7} {'x':>8} {'y':>8} {'yaw':>8}")

    for i in range(n):
        t = i * args.physics_dt
        steer = amp * math.sin(2.0 * math.pi * t / args.period_s)
        norm = S.drive(prim, steer, accelerator=args.throttle,
                       steer_sign=args.steer_sign)
        world.step(render=not args.headless)

        if i % int(0.5 / args.physics_dt) == 0:
            x, y, yaw = S.get_pose()
            print(f"  {t:>6.2f} {math.degrees(steer):>9.2f}d {norm:>7.3f} "
                  f"{x:>8.2f} {y:>8.2f} {math.degrees(yaw):>7.1f}d")

    x, y, yaw = S.get_pose()
    moved = math.hypot(x - x0, y - y0)
    print(f"\nfinal: x={x:.2f} y={y:.2f} yaw={math.degrees(yaw):.1f} deg "
          f"(travelled {moved:.2f} m)")

    # "It ran" is not "it worked". A car steered back and forth must have gone
    # somewhere and swung its heading both ways. Check it, do not eyeball it.
    if moved < 1.0:
        print("\nFAIL: the car barely moved. Either the throttle is not being\n"
              "applied, the handbrake (brake1) is on, or the vehicle prim is\n"
              "not the one actually being simulated. Try --list-vehicle.")
        return 1
    print("\nOK: the vehicle moved and responded to steering.")
    print("Expect a diagonal staircase, NOT a weave about the start line —\n"
          "heading is the integral of steering, so a sine starting at phase\n"
          "zero gives a one-signed heading. That is correct, not a bug.")
    return 0


if __name__ == "__main__":
    code = 1
    try:
        code = main()
    finally:
        sim_app.close()
    sys.exit(code)
