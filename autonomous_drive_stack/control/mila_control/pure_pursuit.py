"""
Pure Pursuit lateral controller.

The whole algorithm is one picture: pick a point on the path a fixed distance
ahead, then steer along the unique circular arc through the car's position and
that point. Everything below is bookkeeping around that idea.

    With the goal expressed in the vehicle frame as (x_v, y_v):

        kappa = 2 * y_v / ld^2                 curvature of that arc
        delta = atan(L * kappa)                steering angle for wheelbase L

Four details decide whether an implementation works or merely runs:

1.  THE REFERENCE POINT IS THE REAR AXLE. Not the centre of mass, not the front
    axle. The derivation puts the vehicle frame's origin at the rear axle,
    because that is the point whose velocity is always along the body x-axis in
    a bicycle model. Use the CG and the car runs with a persistent cross-track
    bias that looks like a tuning problem and isn't.

2.  LOOKAHEAD MUST SCALE WITH SPEED. A fixed ld that is stable at 2 m/s
    oscillates at 8 m/s; one smooth at 8 m/s cuts every corner at 2 m/s.

3.  THE GOAL SEARCH MUST BE FORWARD-ONLY AND BOUNDED. Forward-only stops a
    self-crossing path jumping branches. Bounded stops a closed path matching
    its own endpoint and declaring itself finished immediately. See path.py.

4.  THE ACTUATOR HAS LIMITS. Maximum angle and maximum slew rate. Without them
    you get a controller that is perfect in simulation and saturates on the car.

Known, expected behaviour — be able to state this when asked: Pure Pursuit CUTS
CORNERS. It carries a steady-state cross-track error that grows with ld and with
curvature. That is inherent to the geometry, not a bug, and it is one of the
reasons the team wants MPC.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

from .path import Path


@dataclass
class PurePursuitConfig:
    wheel_base: float = 1.6
    # ld = k*v clamped to [ld_min, ld_max]. Picked from the sweep in
    # tools/verify.py, not guessed: on an 80 m slalom at 6 m/s these give ~0.4 m
    # RMS, where k=0.6 gave 1.3 m by cutting every crest. Retune for the track.
    k_lookahead: float = 0.35        # s
    ld_min: float = 1.5              # m
    ld_max: float = 10.0             # m
    max_steer: float = math.radians(30.0)
    max_steer_rate: float = math.radians(120.0)
    goal_tolerance: float = 1.0      # m


@dataclass
class PurePursuitState:
    last_index: int = 0
    last_steer: float = 0.0
    finished: bool = False


@dataclass
class PurePursuitDebug:
    goal_xy: Optional[np.ndarray]
    goal_index: int
    lookahead: float
    curvature: float
    steer_raw: float
    steer_cmd: float
    alpha: float
    cross_track: float


class PurePursuit:
    def __init__(self, path: Path, cfg: Optional[PurePursuitConfig] = None):
        self.path = path
        self.cfg = cfg or PurePursuitConfig()
        self.state = PurePursuitState()

    def lookahead_for_speed(self, v: float) -> float:
        return float(np.clip(self.cfg.k_lookahead * abs(v),
                             self.cfg.ld_min, self.cfg.ld_max))

    def rear_axle(self, x: float, y: float, yaw: float,
                  from_cg: bool = False) -> Tuple[float, float]:
        """Convert a CG pose to the rear-axle pose the geometry expects.

        Needed when the state estimate is at the centre of mass — Isaac Sim's
        articulation root usually is. Assumes the CG sits at the wheelbase
        midpoint; pass the real offset if you know it.
        """
        if not from_cg:
            return x, y
        d = self.cfg.wheel_base / 2.0
        return x - d * math.cos(yaw), y - d * math.sin(yaw)

    def step(self, x: float, y: float, yaw: float, v: float, dt: float
             ) -> Tuple[float, PurePursuitDebug]:
        """One control cycle. (x, y, yaw) is the REAR AXLE pose in the world
        frame, v is forward speed in m/s, dt the control period in seconds."""
        cfg, st = self.cfg, self.state
        ld = self.lookahead_for_speed(v)
        goal, gi = self.path.lookahead_point(x, y, ld, start=st.last_index)
        xte = self.path.cross_track_error(x, y, start=st.last_index)

        # Path exhausted. Hold the last command and flag it rather than inventing
        # a goal — whoever owns the speed loop needs to know to stop.
        #
        # "Finished" needs BOTH conditions. Proximity alone is wrong on a closed
        # path, whose end sits on its start: the car is within tolerance before
        # it has gone anywhere. Requiring the search to have walked to the end of
        # the array distinguishes "arrived" from "hasn't left yet".
        if goal is None:
            end = self.path.xy[-1]
            near_end = float(np.linalg.norm(end - np.array([x, y]))) < cfg.goal_tolerance
            walked_to_end = st.last_index >= len(self.path.xy) - 2
            if near_end and walked_to_end:
                st.finished = True
            return st.last_steer, PurePursuitDebug(
                None, len(self.path.xy) - 1, ld, 0.0,
                st.last_steer, st.last_steer, 0.0, xte)

        st.last_index = gi

        # World -> vehicle frame; rotating by -yaw puts the body x-axis forward.
        dx, dy = goal[0] - x, goal[1] - y
        cy, sy = math.cos(yaw), math.sin(yaw)
        x_v = dx * cy + dy * sy
        y_v = -dx * sy + dy * cy
        alpha = math.atan2(y_v, x_v)

        # Use the TRUE distance to the goal, not the requested ld: the goal is
        # the first path point AT LEAST ld away, so on a coarse path it can be
        # noticeably further and the curvature would come out wrong.
        ld_actual = math.hypot(x_v, y_v)
        curvature = 2.0 * y_v / (ld_actual ** 2) if ld_actual > 1e-6 else 0.0
        steer_raw = math.atan(cfg.wheel_base * curvature)

        steer = float(np.clip(steer_raw, -cfg.max_steer, cfg.max_steer))
        max_delta = cfg.max_steer_rate * dt
        steer = float(np.clip(steer, st.last_steer - max_delta,
                              st.last_steer + max_delta))
        st.last_steer = steer

        return steer, PurePursuitDebug(goal, gi, ld, curvature,
                                       steer_raw, steer, alpha, xte)

    def reset(self) -> None:
        self.state = PurePursuitState()


def analytic_steer_for_radius(wheel_base: float, radius: float) -> float:
    """delta = atan(L / R): the exact steering angle a kinematic bicycle needs to
    hold a circle of radius R. The known answer tools/verify.py checks against."""
    return math.atan(wheel_base / radius)
