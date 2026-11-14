import copy

import carb.settings
from omni.kit.viewport.utility import get_active_viewport


CAMERA_PATH = "/Mila/MilaCamera"
CAMERA_BINDINGS_SETTING = "/exts/omni.kit.viewport.window/bindings/camera"


def run(runtime):
    viewport = get_active_viewport()
    if viewport is None:
        print("No active viewport; camera was not changed.")
        return

    stage = viewport.stage
    if stage is None or not stage.GetPrimAtPath(CAMERA_PATH).IsValid():
        print(f"Camera not found: {CAMERA_PATH}")
        return

    settings = carb.settings.get_settings()
    runtime["camera_bindings"] = copy.deepcopy(
        settings.get(CAMERA_BINDINGS_SETTING)
    )
    settings.set(CAMERA_BINDINGS_SETTING, {})

    viewport.camera_path = CAMERA_PATH
    runtime["viewport"] = viewport

    print(f"Using locked camera: {CAMERA_PATH}")


def step(runtime):
    """Optional per-frame template hook."""
