import asyncio
import time

import numpy as np
import omni.kit.app
import omni.replicator.core as rep
import omni.usd

from pxr import Semantics

import rclpy
from rclpy.node import Node

from sensor_msgs.msg import Image
from vision_msgs.msg import (
    Detection2DArray,
    Detection2D,
    ObjectHypothesisWithPose,
)
from std_msgs.msg import Header
from builtin_interfaces.msg import Time


# --------------------------------------------------
# Configuration
# --------------------------------------------------

BASE = (
    "/Mila/Mila_Root/SensorStudy/"
    "tn__SensorPlacementModel_wKG/"
    "tn__CameraPlacementStudy_wKG/"
)

CAMERAS = {
    "front": BASE + "oTo_62deg_1/Front",
    "left": BASE + "oTo_120deg/Left",
    "right": BASE + "oTo_62deg/Right",
}

WIDTH = 1280
HEIGHT = 720
PUBLISH_FPS = 3

IMAGE_PREFIX = "/benchmark_camera"
GT_PREFIX = "/ground_truth"


# --------------------------------------------------
# Stop previous execution
# --------------------------------------------------

try:
    GT_TASK.cancel()
except (NameError, AttributeError):
    pass

try:
    GT_NODE.destroy_node()
except (NameError, AttributeError):
    pass

try:
    for annotator in GT_ANNOTATORS:
        annotator.detach()
except (NameError, AttributeError, Exception):
    pass


# --------------------------------------------------
# ROS 2 initialization
# --------------------------------------------------

if not rclpy.ok():
    rclpy.init()

GT_NODE = Node("synchronized_ground_truth_publisher")

image_publishers = {}
gt_publishers = {}

for name in CAMERAS:
    image_publishers[name] = GT_NODE.create_publisher(
        Image,
        f"{IMAGE_PREFIX}/{name}/image_raw",
        10,
    )

    gt_publishers[name] = GT_NODE.create_publisher(
        Detection2DArray,
        f"{GT_PREFIX}/{name}",
        10,
    )


# --------------------------------------------------
# Label people
# --------------------------------------------------

stage = omni.usd.get_context().get_stage()

person_paths = ["/ParkingLot/Xform"]

people_root = stage.GetPrimAtPath(
    "/ParkingLot/BenchmarkHumans"
)

if people_root.IsValid():
    for child in people_root.GetChildren():
        person_paths.append(str(child.GetPath()))

labeled = 0

for path in person_paths:
    prim = stage.GetPrimAtPath(path)

    if not prim.IsValid():
        continue

    # FIX: Use the USD prim path, not a Python string.
    sem = Semantics.SemanticsAPI.Get(
        prim,
        "Semantics",
    )

    if not sem:
        sem = Semantics.SemanticsAPI.Apply(
            prim,
            "Semantics",
        )

    sem.CreateSemanticTypeAttr().Set("class")
    sem.CreateSemanticDataAttr().Set("person")

    labeled += 1

print(f"Labeled {labeled} person prims")


# --------------------------------------------------
# Create render products
# --------------------------------------------------

render_products = {}
rgb_annotators = {}
bbox_annotators = {}
GT_ANNOTATORS = []

for name, camera_path in CAMERAS.items():

    camera_prim = stage.GetPrimAtPath(camera_path)

    if not camera_prim.IsValid():
        raise RuntimeError(
            f"Camera not found: {camera_path}"
        )

    render_product = rep.create.render_product(
        camera_path,
        (WIDTH, HEIGHT),
    )

    rgb = rep.AnnotatorRegistry.get_annotator("rgb")
    bbox = rep.AnnotatorRegistry.get_annotator(
        "bounding_box_2d_tight"
    )

    rgb.attach([render_product])
    bbox.attach([render_product])

    render_products[name] = render_product
    rgb_annotators[name] = rgb
    bbox_annotators[name] = bbox

    GT_ANNOTATORS.extend([rgb, bbox])


# --------------------------------------------------
# ROS timestamp
# --------------------------------------------------

def make_stamp():
    ns = time.time_ns()

    stamp = Time()
    stamp.sec = ns // 1_000_000_000
    stamp.nanosec = ns % 1_000_000_000

    return stamp


# --------------------------------------------------
# Convert RGB image to ROS Image
# --------------------------------------------------

def make_image(rgb_data, stamp, camera):

    pixels = np.asarray(rgb_data)

    if pixels.ndim != 3:
        return None

    if pixels.shape[2] == 4:
        pixels = pixels[:, :, :3]

    pixels = np.ascontiguousarray(
        pixels.astype(np.uint8)
    )

    height, width = pixels.shape[:2]

    msg = Image()
    msg.header = Header()
    msg.header.stamp = stamp
    msg.header.frame_id = camera

    msg.height = height
    msg.width = width
    msg.encoding = "rgb8"
    msg.is_bigendian = 0
    msg.step = width * 3
    msg.data = pixels.tobytes()

    return msg


# --------------------------------------------------
# Convert bounding boxes to ROS detections
# --------------------------------------------------

def make_boxes(result, stamp, camera):

    msg = Detection2DArray()
    msg.header.stamp = stamp
    msg.header.frame_id = camera

    if result is None:
        return msg

    rows = result.get("data", [])
    labels = result.get("info", {}).get(
        "idToLabels", {}
    )

    for row in rows:

        semantic_id = int(row["semanticId"])

        label_data = labels.get(
            str(semantic_id),
            labels.get(semantic_id, {}),
        )

        if label_data.get("class") != "person":
            continue

        x1 = float(row["x_min"])
        y1 = float(row["y_min"])
        x2 = float(row["x_max"])
        y2 = float(row["y_max"])

        if x2 <= x1 or y2 <= y1:
            continue

        det = Detection2D()
        det.header = msg.header

        det.bbox.center.position.x = (x1 + x2) / 2
        det.bbox.center.position.y = (y1 + y2) / 2

        det.bbox.size_x = x2 - x1
        det.bbox.size_y = y2 - y1

        hypothesis = ObjectHypothesisWithPose()
        hypothesis.hypothesis.class_id = "person"
        hypothesis.hypothesis.score = 1.0

        det.results.append(hypothesis)
        msg.detections.append(det)

    return msg


# --------------------------------------------------
# Publish RGB and ground truth together
# --------------------------------------------------

async def publish_synchronized():

    print("Synchronized publisher started")

    frame_count = 0
    interval = 1.0 / PUBLISH_FPS
    last_publish = 0.0

    while True:

        await omni.kit.app.get_app().next_update_async()

        now = time.monotonic()

        if now - last_publish < interval:
            continue

        last_publish = now
        stamp = make_stamp()

        for camera in CAMERAS:

            rgb_data = rgb_annotators[
                camera
            ].get_data()

            bbox_data = bbox_annotators[
                camera
            ].get_data()

            if rgb_data is None or bbox_data is None:
                continue

            image_msg = make_image(
                rgb_data,
                stamp,
                camera,
            )

            if image_msg is None:
                continue

            gt_msg = make_boxes(
                bbox_data,
                stamp,
                camera,
            )

            image_publishers[camera].publish(
                image_msg
            )

            gt_publishers[camera].publish(
                gt_msg
            )

        frame_count += 1

        if frame_count % 30 == 0:
            print(
                f"Published {frame_count} "
                "capture batches"
            )


# --------------------------------------------------
# Start
# --------------------------------------------------

GT_TASK = asyncio.ensure_future(
    publish_synchronized()
)

print("Ground-truth publisher ready")




