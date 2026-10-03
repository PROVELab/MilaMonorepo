"""
Kinematic bicycle model — the plant the controller is tested against.

Two wheels, one steered. It ignores tyre slip and load transfer, which is
exactly why it is right here: if Pure Pursuit cannot track a path on a bicycle
model, the problem is the controller, not the vehicle. Get it right here, then
let Isaac Sim's PhysX add the dynamics.

State, all at the REAR AXLE (the same reference point the controller uses):

    x'     = v cos(theta)
    y'     = v sin(theta)
    theta' = (v / L) tan(delta)
    v'     = a
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Tuple

import numpy as np


@dataclass
class VehicleParams:
    """The minimum set that describes a steered car.

    The assignment asked for the minimum number of parameters for a basic
    simulation, and this is it. Mass, inertia and tyre parameters only matter
    once you care about forces, which a kinematic model does not. Replace these
    defaults with Mila's real geometry when it is measured.
    """
    wheel_base: float = 1.6              # m, front axle to rear axle
    track_width: float = 1.2             # m, left wheel to right wheel
    wheel_radius: float = 0.23           # m
    max_steer: float = math.radians(30)  # rad
    max_speed: float = 15.0              # m/s
    max_accel: float = 3.0               # m/s^2


@dataclass
class VehicleState:
    x: float = 0.0      # m, rear axle, world frame
    y: float = 0.0
    yaw: float = 0.0    # rad, 0 = +x
    v: float = 0.0      # m/s, forward

    def as_array(self) -> np.ndarray:
        return np.array([self.x, self.y, self.yaw, self.v], dtype=np.float64)


def _derivative(s: np.ndarray, steer: float, accel: float, L: float) -> np.ndarray:
    x, y, yaw, v = s
    return np.array([v * math.cos(yaw), v * math.sin(yaw),
                     v / L * math.tan(steer), accel], dtype=np.float64)


class KinematicBicycle:
    """RK4-integrated bicycle.

    Why RK4 rather than forward Euler: on a curved path Euler's truncation error
    shows up as a steady outward drift that looks exactly like controller
    tracking error, and you lose an afternoon tuning a controller that was
    already correct. RK4 is ten extra lines and deletes that whole class of
    confusion.
    """

    def __init__(self, params: VehicleParams | None = None,
                 state: VehicleState | None = None):
        self.p = params or VehicleParams()
        self.state = state or VehicleState()

    def step(self, steer_cmd: float, accel_cmd: float, dt: float) -> VehicleState:
        p = self.p
        steer = float(np.clip(steer_cmd, -p.max_steer, p.max_steer))
        accel = float(np.clip(accel_cmd, -p.max_accel, p.max_accel))

        s = self.state.as_array()
        f = lambda y: _derivative(y, steer, accel, p.wheel_base)
        k1 = f(s); k2 = f(s + 0.5 * dt * k1)
        k3 = f(s + 0.5 * dt * k2); k4 = f(s + dt * k3)
        s = s + (dt / 6.0) * (k1 + 2 * k2 + 2 * k3 + k4)

        s[3] = float(np.clip(s[3], 0.0, p.max_speed))
        s[2] = math.atan2(math.sin(s[2]), math.cos(s[2]))
        self.state = VehicleState(float(s[0]), float(s[1]), float(s[2]), float(s[3]))
        return self.state

    def front_axle(self) -> Tuple[float, float]:
        s, p = self.state, self.p
        return (s.x + p.wheel_base * math.cos(s.yaw),
                s.y + p.wheel_base * math.sin(s.yaw))

    def ackermann_wheel_angles(self, steer: float) -> Tuple[float, float]:
        """Split one virtual bicycle steering angle into left and right wheel
        angles. A real steered axle cannot give both wheels the same angle — the
        inner wheel traces a tighter circle and must turn further, or it scrubs.
        Isaac's AckermannController does this internally; it is here so the two
        models agree on what `steer` means."""
        if abs(steer) < 1e-6:
            return 0.0, 0.0
        L, W = self.p.wheel_base, self.p.track_width
        R = L / math.tan(steer)
        inner = math.atan(L / (R - math.copysign(W / 2.0, R)))
        outer = math.atan(L / (R + math.copysign(W / 2.0, R)))
        return (inner, outer) if steer > 0 else (outer, inner)


def speed_controller(v: float, v_target: float, kp: float = 1.2) -> float:
    """Deliberately trivial longitudinal control. The assignment is about
    lateral control; keeping this simple makes it obvious that any tracking
    error you see is the steering, not the throttle."""
    return kp * (v_target - v)
