import omni.kit.app

from isaacsim.sensors.experimental.rtx import (
    LidarSensor,
    parse_generic_model_output_data,
)


LIDAR_PATH = "/Mila/Mila_Root/OS1_01/sensor"


def run(runtime):
    print("Starting LiDAR setup...")

    extension_manager = omni.kit.app.get_app().get_extension_manager()

    extension_manager.set_extension_enabled_immediate(
        "isaacsim.sensors.rtx.nodes",
        True,
    )

    # Wait a couple of Isaac frames before constructing the sensor wrapper.
    # This gives the newly enabled extension time to register its writer.
    runtime["lidar"] = None
    runtime["lidar_setup_frames"] = 0
    runtime["frame_count"] = 0
    runtime["last_gmo"] = None


def step(runtime):
    # Wait for isaacsim.sensors.rtx.nodes to finish loading.
    if runtime["lidar"] is None:
        runtime["lidar_setup_frames"] += 1

        if runtime["lidar_setup_frames"] < 3:
            return

        print("Creating LiDAR point-cloud visualizer...")

        runtime["lidar"] = LidarSensor(
            LIDAR_PATH,
            annotators=["generic-model-output"],
        )

        runtime["lidar"].attach_writer(
            "draw-point-cloud",
            color=[0.0, 0.8, 1.0, 1.0],
            size=0.025,
        )

        return

    lidar = runtime["lidar"]

    try:
        data, info = lidar.get_data("generic-model-output")

        if data is None:
            return

        runtime["last_gmo"] = parse_generic_model_output_data(data)

    except Exception:
        return

    runtime["frame_count"] += 1