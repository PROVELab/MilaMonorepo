"""PhysX vehicle IMU and magnetometer drive test.

Run in the Isaac Sim Script Editor or via the Isaac Sim VS Code extension.

Sequence:
    gentle straight acceleration
    left turn
    straight
    right turn
    brake
    release controls

This controls the vehicle's PhysX accelerator, steering, and brake inputs.
It does not directly set the chassis velocity.
"""

import omni.timeline
import omni.usd

from isaacsim.core.simulation_manager import SimulationManager
from isaacsim.core.simulation_manager.impl.isaac_events import IsaacEvents


# Vehicle and test settings.
CAR_PATH = "/Mila/Mila_Root"

ACCELERATOR = 0.10

# You confirmed positive steer turns left.
# Increase gradually if the turns are too subtle.
STEER_AMOUNT = 0.7

# Start gently because Brake 0 stopped your car quickly.
BRAKE_AMOUNT = 0.03

# Durations are in SIMULATION seconds, not necessarily wall-clock seconds.
STRAIGHT_SECONDS = 3.5
LEFT_SECONDS = 4.0
BETWEEN_SECONDS = 2.0
RIGHT_SECONDS = 4.0
BRAKE_SECONDS = 5.0


# Stop a callback left by an earlier run of THIS script, if present.
old_callback_id = globals().get("_mila_vehicle_test_callback_id")
if old_callback_id is not None:
    SimulationManager.deregister_callback(old_callback_id)
    _mila_vehicle_test_callback_id = None


stage = omni.usd.get_context().get_stage()
if stage is None:
    raise RuntimeError("No stage is open in Isaac Sim.")

vehicle = stage.GetPrimAtPath(CAR_PATH)
if not vehicle.IsValid():
    raise RuntimeError(f"Vehicle prim not found: {CAR_PATH}")


def get_existing_attribute(name):
    attribute = vehicle.GetAttribute(name)
    if not attribute:
        raise RuntimeError(
            f"Missing vehicle controller attribute '{name}' on {CAR_PATH}"
        )
    return attribute


accelerator_attr = get_existing_attribute("physxVehicleController:accelerator")
steer_attr = get_existing_attribute("physxVehicleController:steer")
brake0_attr = get_existing_attribute("physxVehicleController:brake0")
brake1_attr = get_existing_attribute("physxVehicleController:brake1")


def set_controls(accelerator, steer, brake):
    """Set normalized controller inputs."""
    accelerator_attr.Set(float(accelerator))
    steer_attr.Set(float(steer))
    brake0_attr.Set(float(brake))
    brake1_attr.Set(0.0)


# Make sure we don't start with leftover manual inputs.
set_controls(0.0, 0.0, 0.0)

state = {
    "elapsed": 0.0,
    "phase": None,
}

left_start = STRAIGHT_SECONDS
between_start = left_start + LEFT_SECONDS
right_start = between_start + BETWEEN_SECONDS
brake_start = right_start + RIGHT_SECONDS
finish_time = brake_start + BRAKE_SECONDS


def report_phase(name):
    if state["phase"] != name:
        state["phase"] = name
        print(f"Vehicle test: {name} (sim time: {state['elapsed']:.1f} s)")


def drive_callback(dt, _context):
    global _mila_vehicle_test_callback_id

    state["elapsed"] += dt
    t = state["elapsed"]

    if t < left_start:
        report_phase("straight")
        set_controls(ACCELERATOR, 0.0, 0.0)

    elif t < between_start:
        report_phase("left turn")
        set_controls(ACCELERATOR, STEER_AMOUNT, 0.0)

    elif t < right_start:
        report_phase("straight between turns")
        set_controls(ACCELERATOR, 0.0, 0.0)

    elif t < brake_start:
        report_phase("right turn")
        set_controls(ACCELERATOR, -STEER_AMOUNT, 0.0)

    elif t < finish_time:
        report_phase("braking")
        set_controls(0.0, 0.0, BRAKE_AMOUNT)

    else:
        set_controls(0.0, 0.0, 0.0)
        report_phase("complete; controls released")

        callback_id = _mila_vehicle_test_callback_id
        _mila_vehicle_test_callback_id = None
        if callback_id is not None:
            SimulationManager.deregister_callback(callback_id)


_mila_vehicle_test_callback_id = SimulationManager.register_callback(
    drive_callback,
    IsaacEvents.POST_PHYSICS_STEP,
)

omni.timeline.get_timeline_interface().play()

print(
    "Vehicle test started. Use Isaac Sim's Stop button if needed. "
    "Do not run another vehicle controller at the same time."
)