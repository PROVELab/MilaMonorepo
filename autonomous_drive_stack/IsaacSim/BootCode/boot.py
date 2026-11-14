import sys
import types
from pathlib import Path

import omni.timeline
import omni.kit.app


SCRIPT_FOLDER = Path(__file__).resolve().parent


# If boot.py is run again while Play is active, shut down the previous run
# before replacing its subscriptions. This lets on_stop release anything
# created by on_play.
try:
    previous_control_sub = control_sub
    if previous_control_sub is not None:
        previous_control_sub.unsubscribe()
except (NameError, AttributeError):
    pass

try:
    previous_runtime = runtime
    previous_stop_module = load_file("on_stop")
    previous_stop_module.run(previous_runtime)
    previous_runtime.clear()
except (NameError, AttributeError):
    pass

try:
    timeline_sub.unsubscribe()
except (NameError, AttributeError):
    pass


if str(SCRIPT_FOLDER) not in sys.path:
    sys.path.insert(0, str(SCRIPT_FOLDER))


runtime = {}
play_module = None
control_sub = None


def load_file(module_name):
    file_path = SCRIPT_FOLDER / f"{module_name}.py"

    if not file_path.exists():
        raise FileNotFoundError(f"Missing script: {file_path}")

    module = types.ModuleType(module_name)
    module.__file__ = str(file_path)

    source = file_path.read_text(encoding="utf-8")
    exec(compile(source, str(file_path), "exec"), module.__dict__)

    return module


def on_app_update(event):
    if play_module is not None and hasattr(play_module, "step"):
        play_module.step(runtime)


def start_run():
    global play_module, control_sub

    # Do not create another loop when resuming from Pause.
    if control_sub is not None:
        return

    play_module = load_file("on_play")
    play_module.run(runtime)

    control_sub = (
        omni.kit.app.get_app()
        .get_update_event_stream()
        .create_subscription_to_pop(
            on_app_update,
            name="MilaAutonomyLoop",
        )
    )

    print("Autonomy started.")


def stop_run():
    global play_module, control_sub

    if control_sub is not None:
        control_sub.unsubscribe()
        control_sub = None

    stop_module = load_file("on_stop")
    stop_module.run(runtime)

    runtime.clear()
    play_module = None

    print("Autonomy stopped.")


def on_timeline_event(event):
    if event.type == int(omni.timeline.TimelineEventType.PLAY):
        start_run()

    elif event.type == int(omni.timeline.TimelineEventType.STOP):
        stop_run()


timeline = omni.timeline.get_timeline_interface()

timeline_sub = (
    timeline.get_timeline_event_stream()
    .create_subscription_to_pop(
        on_timeline_event,
        name="MilaPlayStopManager",
    )
)

print("Boot ready.")
