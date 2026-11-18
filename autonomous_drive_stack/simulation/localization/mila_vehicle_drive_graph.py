
"""Drive the MILA vehicle when the stage is playing."""

import omni.timeline
import omni.usd


CAR_PATH = "/Mila/Mila_Root"

ACCELERATOR = 0.10
STEER_AMOUNT = 0.7
BRAKE_AMOUNT = 0.03

STRAIGHT_SECONDS = 3.5
LEFT_SECONDS = 4.0
BETWEEN_SECONDS = 2.0
RIGHT_SECONDS = 4.0
BRAKE_SECONDS = 5.0


def set_controls(state, accelerator, steer, brake):
    state.accelerator.Set(float(accelerator))
    state.steer.Set(float(steer))
    state.brake0.Set(float(brake))
    state.brake1.Set(0.0)


def reset_sequence(state):
    state.elapsed = 0.0
    state.phase = None
    state.finished = False
    set_controls(state, 0.0, 0.0, 0.0)


def report_phase(state, name):
    if state.phase != name:
        state.phase = name
        print(
            f"Vehicle test: {name} "
            f"(sim time: {state.elapsed:.1f} s)"
        )


def setup(db):
    state = db.per_instance_state

    stage = omni.usd.get_context().get_stage()
    if stage is None:
        raise RuntimeError("No stage is open.")

    state.vehicle = stage.GetPrimAtPath(CAR_PATH)

    if not state.vehicle.IsValid():
        raise RuntimeError(
            f"Vehicle prim not found: {CAR_PATH}"
        )

    def get_attribute(name):
        attr = state.vehicle.GetAttribute(name)

        if not attr:
            raise RuntimeError(
                f"Missing vehicle controller attribute: {name}"
            )

        return attr

    state.accelerator = get_attribute(
        "physxVehicleController:accelerator"
    )

    state.steer = get_attribute(
        "physxVehicleController:steer"
    )

    state.brake0 = get_attribute(
        "physxVehicleController:brake0"
    )

    state.brake1 = get_attribute(
        "physxVehicleController:brake1"
    )

    reset_sequence(state)

    def on_timeline_event(event):
        if event.type == int(
            omni.timeline.TimelineEventType.STOP
        ):
            if state.vehicle.IsValid():
                reset_sequence(state)
                print(
                    "Vehicle test reset; controls released."
                )

    state.timeline_subscription = (
        omni.timeline.get_timeline_interface()
        .get_timeline_event_stream()
        .create_subscription_to_pop(
            on_timeline_event,
            name="MILA stage drive test Stop",
        )
    )

    print("MILA stage drive test initialized.")


def compute(db):
    state = db.per_instance_state

    if not state.vehicle.IsValid():
        db.log_error("MILA vehicle prim is no longer valid.")
        return False

    if state.finished:
        return True

    state.elapsed += float(db.inputs.deltaTime)
    t = state.elapsed

    left_start = STRAIGHT_SECONDS
    between_start = left_start + LEFT_SECONDS
    right_start = between_start + BETWEEN_SECONDS
    brake_start = right_start + RIGHT_SECONDS
    finish_time = brake_start + BRAKE_SECONDS

    if t < left_start:
        report_phase(state, "straight")
        set_controls(
            state,
            ACCELERATOR,
            0.0,
            0.0,
        )

    elif t < between_start:
        report_phase(state, "left turn")
        set_controls(
            state,
            ACCELERATOR,
            STEER_AMOUNT,
            0.0,
        )

    elif t < right_start:
        report_phase(state, "straight between turns")
        set_controls(
            state,
            ACCELERATOR,
            0.0,
            0.0,
        )

    elif t < brake_start:
        report_phase(state, "right turn")
        set_controls(
            state,
            ACCELERATOR,
            -STEER_AMOUNT,
            0.0,
        )

    elif t < finish_time:
        report_phase(state, "braking")
        set_controls(
            state,
            0.0,
            0.0,
            BRAKE_AMOUNT,
        )

    else:
        set_controls(state, 0.0, 0.0, 0.0)
        report_phase(state, "complete; controls released")
        state.finished = True

    return True


def cleanup(db):
    state = db.per_instance_state

    if (
        hasattr(state, "vehicle")
        and state.vehicle.IsValid()
        and hasattr(state, "accelerator")
    ):
        set_controls(state, 0.0, 0.0, 0.0)

    if hasattr(state, "timeline_subscription"):
        state.timeline_subscription = None
