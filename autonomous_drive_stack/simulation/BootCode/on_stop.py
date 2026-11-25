import carb.settings
from omni.kit.viewport.utility import get_active_viewport


PERSPECTIVE_CAMERA_PATH = "/OmniverseKit_Persp"
CAMERA_BINDINGS_SETTING = "/exts/omni.kit.viewport.window/bindings/camera"


def run(runtime):
    viewport = runtime.get("viewport") or get_active_viewport()
    if viewport is not None:
        viewport.camera_path = PERSPECTIVE_CAMERA_PATH

    if "camera_bindings" in runtime:
        carb.settings.get_settings().set(
            CAMERA_BINDINGS_SETTING,
            runtime["camera_bindings"],
        )

    print("Returned to the Perspective camera.")
