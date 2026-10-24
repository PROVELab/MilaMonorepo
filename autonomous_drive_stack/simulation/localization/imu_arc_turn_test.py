"""Forward-moving arc-turn IMU test for Isaac Sim 6.1.

Run this through the Isaac Sim VS Code extension or Script Editor while the
vehicle stage is open. It approximates a regular car's path by commanding a
forward speed v and yaw rate omega = v / R. The chassis therefore follows
forward-moving arcs of radius R, instead of sliding sideways or spinning.

This is a kinematic sensor-validation tool; it does not steer physical wheel
joints. A wheel-accurate controller needs the vehicle articulation's steering
and wheel-joint paths.
"""

import numpy as np
import omni.timeline
import omni.usd
from pxr import UsdPhysics

from isaacsim.core.experimental.prims import RigidPrim
from isaacsim.core.simulation_manager import SimulationManager
from isaacsim.core.simulation_manager.impl.isaac_events import IsaacEvents
from isaacsim.core.rendering_manager import RenderingManager

TARGET_HZ = 416

# Physics step interval
SimulationManager.setup_simulation(dt=1.0 / TARGET_HZ)

# Isaac application's update / rate-limit target
RenderingManager.set_dt(1.0 / TARGET_HZ)

print(f"Configured physics and application target rate: {TARGET_HZ} Hz")


# Change only these if needed.
CAR_RIGID_BODY_PATH = "/MILA_substitute"
MAX_SPEED_MPS = 1.0
TURN_RADIUS_M = 4.0

# Your local +X test moved sideways. The vehicle's longitudinal axis should be
# local +Z. If this first test drives backward, change the final 1.0 to -1.0.
FORWARD_AXIS_LOCAL = np.array([0.0, 0.0, 1.0], dtype=np.float32)
# We previously verified this vehicle/sensor setup uses local +Y as up.
UP_AXIS_LOCAL = np.array([0.0, 1.0, 0.0], dtype=np.float32)

SPEED_RAMP_SECONDS = 2.0
STRAIGHT_BEFORE_SECONDS = 2.0
TURN_RAMP_SECONDS = 1.0
TURN_HOLD_SECONDS = 3.0
BETWEEN_TURNS_SECONDS = 2.0
STRAIGHT_AFTER_SECONDS = 2.0


def smoothstep(u: float) -> float:
    u = max(0.0, min(1.0, u))
    return u * u * (3.0 - 2.0 * u)


def rotation_matrix_from_wxyz(q: np.ndarray) -> np.ndarray:
    """Return the body-to-world rotation matrix for normalized [w,x,y,z]."""
    w, x, y, z = q / np.linalg.norm(q)
    return np.array(
        [
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
        ],
        dtype=np.float32,
    )


ONE_TURN_SECONDS = 2.0 * TURN_RAMP_SECONDS + TURN_HOLD_SECONDS
TOTAL_SECONDS = (
    SPEED_RAMP_SECONDS
    + STRAIGHT_BEFORE_SECONDS
    + ONE_TURN_SECONDS
    + BETWEEN_TURNS_SECONDS
    + ONE_TURN_SECONDS
    + STRAIGHT_AFTER_SECONDS
    + SPEED_RAMP_SECONDS
)


def turn_shape(local_t: float) -> float:
    """Smooth 0 -> 1 -> 0 turn-rate shape over one turn interval."""
    if local_t < TURN_RAMP_SECONDS:
        return smoothstep(local_t / TURN_RAMP_SECONDS)
    if local_t < TURN_RAMP_SECONDS + TURN_HOLD_SECONDS:
        return 1.0
    if local_t < ONE_TURN_SECONDS:
        return 1.0 - smoothstep(
            (local_t - TURN_RAMP_SECONDS - TURN_HOLD_SECONDS) / TURN_RAMP_SECONDS
        )
    return 0.0


def command_profile(t: float) -> tuple[float, float]:
    """Return forward speed [m/s] and yaw rate [rad/s] at time t."""
    if t < SPEED_RAMP_SECONDS:
        speed = MAX_SPEED_MPS * smoothstep(t / SPEED_RAMP_SECONDS)
    elif t < TOTAL_SECONDS - SPEED_RAMP_SECONDS:
        speed = MAX_SPEED_MPS
    elif t < TOTAL_SECONDS:
        speed = MAX_SPEED_MPS * (
            1.0 - smoothstep((t - (TOTAL_SECONDS - SPEED_RAMP_SECONDS)) / SPEED_RAMP_SECONDS)
        )
    else:
        return 0.0, 0.0

    # omega = v / R produces a circular path of radius R.
    base_yaw_rate = speed / TURN_RADIUS_M
    left_turn_start = SPEED_RAMP_SECONDS + STRAIGHT_BEFORE_SECONDS
    right_turn_start = left_turn_start + ONE_TURN_SECONDS + BETWEEN_TURNS_SECONDS

    if left_turn_start <= t < left_turn_start + ONE_TURN_SECONDS:
        return speed, base_yaw_rate * turn_shape(t - left_turn_start)
    if right_turn_start <= t < right_turn_start + ONE_TURN_SECONDS:
        return speed, -base_yaw_rate * turn_shape(t - right_turn_start)
    return speed, 0.0


# If this script is re-run, stop its earlier callback first.
old_callback_id = globals().get("_imu_arc_callback_id")
if old_callback_id is not None:
    SimulationManager.deregister_callback(old_callback_id)

stage = omni.usd.get_context().get_stage()
car_usd_prim = stage.GetPrimAtPath(CAR_RIGID_BODY_PATH)
if not car_usd_prim.IsValid():
    raise RuntimeError(f"No prim exists at {CAR_RIGID_BODY_PATH}")
if not car_usd_prim.HasAPI(UsdPhysics.RigidBodyAPI):
    raise RuntimeError(f"{CAR_RIGID_BODY_PATH} is not a rigid body.")

car = RigidPrim(CAR_RIGID_BODY_PATH)
state = {"elapsed": 0.0, "finished": False}


def drive_callback(dt: float, _context) -> None:
    state["elapsed"] += dt
    speed, yaw_rate = command_profile(state["elapsed"])
    _, orientations = car.get_world_poses()
    body_to_world = rotation_matrix_from_wxyz(np.asarray(orientations[0], dtype=np.float32))

    linear_world = body_to_world @ (speed * FORWARD_AXIS_LOCAL)
    angular_world = body_to_world @ (yaw_rate * UP_AXIS_LOCAL)
    car.set_velocities(
        linear_velocities=np.asarray([linear_world], dtype=np.float32),
        angular_velocities=np.asarray([angular_world], dtype=np.float32),
    )

    if not state["finished"] and state["elapsed"] >= TOTAL_SECONDS:
        state["finished"] = True
        print("Arc-turn IMU test complete: vehicle is stopped.")


_imu_arc_callback_id = SimulationManager.register_callback(
    drive_callback, IsaacEvents.POST_PHYSICS_STEP
)

omni.timeline.get_timeline_interface().play()
print(
    "Arc-turn IMU test started: ramp -> straight -> arc A -> straight -> arc B -> stop. "
    "Do not drag the vehicle while it runs."
)
