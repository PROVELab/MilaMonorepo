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
    """Mila's real geometry, read out of the simulation asset.

    SOURCE: PROVELab/autonomous-drive-stack,
            simulation/parking_lot/mila_vehicle.usd, prim /Mila/Mila_Root
            (sha256 6831b62e…b44e4e2, 7,044,754 bytes)

    That asset is a PhysX Vehicle, not a jointed articulation — which is why
    searching the monorepo for ArticulationRoot and revolute joints found
    nothing. The numbers below are copied from its attributes, not measured and
    not assumed:

        physxVehicleAckermannSteering:wheelBase      = 2.78
        physxVehicleAckermannSteering:trackWidth     = 1.73
        physxVehicleAckermannSteering:maxSteerAngle  = 0.5     (rad)
        physxVehicleWheel:radius                     = 0.35025
        physics:mass                                 = 907.2
        physics:centerOfMass                         = (-0.139, 0, 0.4)

    >>> OPEN QUESTION FOR THE TEAM: is mila_vehicle.usd the canonical vehicle? <<<
    It lives in autonomous-drive-stack, NOT in MilaMonorepo, and the monorepo's
    own stage (FirstMilaStage_light.usda) still loads the Buick placeholder
    instead. The two repos also disagree on Isaac version — 6.1 there, 6.0 in
    the monorepo. If a newer or different vehicle definition is authoritative,
    these six numbers are the only thing that has to change.
    """
    wheel_base: float = 2.78             # m, front axle to rear axle
    track_width: float = 1.73            # m, left wheel to right wheel
    wheel_radius: float = 0.35025        # m
    max_steer: float = 0.5               # rad = 28.65 deg

    # Distance from centre of mass to the REAR AXLE. Not wheel_base/2: the CoM
    # sits at x = -0.139 while the rear axle is at x = -1.39, so 1.251 m. Pure
    # Pursuit needs this to convert a CG pose into the rear-axle pose its
    # geometry is derived about.
    cg_to_rear_axle: float = 1.251       # m

    mass: float = 907.2                  # kg, sprung mass (excludes 4 x 15 kg wheels)

    # Not in the USD. max_accel is derived: the drive is rear-wheel with
    # physxVehicleDriveBasic:peakTorque = 1000 N.m, so F = T/r = 1000/0.35025
    # = 2855 N and a = F/m = 2855/907.2 = 3.15 m/s^2. Rounded down to 3.0.
    # max_speed is still a genuine placeholder — nothing in the asset sets it.
    max_speed: float = 15.0              # m/s   <-- PLACEHOLDER, needs a real figure
    max_accel: float = 3.0               # m/s^2, derived from peak torque

    # --- Dynamic-model parameters -------------------------------------------
    # Not used by the kinematic bicycle or by either controller today. They are
    # recorded here because they are in the asset and would otherwise have to be
    # re-derived, and because a dynamic-model MPC would need exactly these.
    #
    # cg_to_front_axle + cg_to_rear_axle = 1.529 + 1.251 = 2.780, which matches
    # the declared wheelbase exactly — an independent check that the wheel
    # positions were read correctly.
    cg_to_front_axle: float = 1.529      # m
    yaw_inertia: float = 1554.336        # kg.m^2, physics:diagonalInertia z

    # Cornering stiffness per AXLE (2 wheels x physxVehicleTire:lateralStiffness).
    #
    # >>> WARNING: THESE ARE SYNTHETIC, NOT MEASURED. <<<
    # Front and rear lateral stiffness are *exactly* proportional to static
    # sprung mass — 35943.4/204.12 = 43930.8/249.48 = 176.090 N/rad per kg — a
    # ratio of 1.000000. That is the signature of Isaac's vehicle wizard scaling
    # stiffness by load when nobody supplies real tyre data.
    #
    # The consequence: the understeer gradient K = (m/L)(lr/Cf - lf/Cr) comes out
    # at zero to one part in 1e5, so the car is EXACTLY neutral-steer. Real cars
    # are deliberately built to understeer, because that is stable at any speed
    # while a neutral car sits exactly on the stability boundary. Anything tuned
    # against these numbers will not transfer to the real vehicle.
    #
    # This is why the MPC in mpc.py uses the KINEMATIC model: it does not use
    # cornering stiffness at all, so it cannot inherit this assumption.
    cornering_stiffness_front: float = 71886.8   # N/rad  <-- synthetic
    cornering_stiffness_rear: float = 87861.6    # N/rad  <-- synthetic


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
