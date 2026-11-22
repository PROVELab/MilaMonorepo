
import argparse
import time
from collections import OrderedDict

import cv2
import numpy as np
import rclpy

from rclpy.node import Node
from sensor_msgs.msg import Image
from vision_msgs.msg import Detection2DArray
from ultralytics import YOLO


CAMERAS = ["front", "left", "right"]

# New synchronized image topics
IMAGE_PREFIX = "/benchmark_camera"
GT_PREFIX = "/ground_truth"

MODEL = "cone_v11m.pt"
CONFIDENCE = 0.25
IMAGE_SIZE = 640
IOU_THRESHOLD = 0.50

MIN_GT_INSTANCES = 1000
MIN_RECALL = 0.95
MAX_FDR = 0.05

MAX_CACHE = 100


def timestamp(msg):
    return (
        msg.header.stamp.sec,
        msg.header.stamp.nanosec,
    )


def image_to_bgr(msg):
    if msg.encoding not in ("rgb8", "bgr8"):
        raise ValueError(
            f"Unsupported image encoding: {msg.encoding}"
        )

    raw = np.frombuffer(msg.data, dtype=np.uint8)

    image = raw.reshape(
        msg.height,
        msg.step,
    )[:, :msg.width * 3]

    image = image.reshape(
        msg.height,
        msg.width,
        3,
    )

    if msg.encoding == "rgb8":
        image = cv2.cvtColor(
            image,
            cv2.COLOR_RGB2BGR,
        )

    return np.ascontiguousarray(image)


def gt_boxes(msg):
    boxes = []

    for detection in msg.detections:
        b = detection.bbox

        x1 = b.center.position.x - b.size_x / 2
        y1 = b.center.position.y - b.size_y / 2
        x2 = b.center.position.x + b.size_x / 2
        y2 = b.center.position.y + b.size_y / 2

        if x2 > x1 and y2 > y1:
            boxes.append([x1, y1, x2, y2])

    return np.asarray(boxes, dtype=np.float32).reshape(-1, 4)


def calculate_iou(a, b):
    x1 = max(a[0], b[0])
    y1 = max(a[1], b[1])
    x2 = min(a[2], b[2])
    y2 = min(a[3], b[3])

    intersection = (
        max(0, x2 - x1)
        * max(0, y2 - y1)
    )

    area_a = max(0, a[2] - a[0]) * max(
        0, a[3] - a[1]
    )

    area_b = max(0, b[2] - b[0]) * max(
        0, b[3] - b[1]
    )

    union = area_a + area_b - intersection

    return intersection / union if union > 0 else 0.0


def match_boxes(predictions, ground_truth):
    candidates = []

    for pi, pred in enumerate(predictions):
        for gi, gt in enumerate(ground_truth):
            iou = calculate_iou(pred, gt)

            if iou >= IOU_THRESHOLD:
                candidates.append((iou, pi, gi))

    candidates.sort(reverse=True)

    matched_predictions = set()
    matched_gt = set()
    matched_ious = []

    for iou, pi, gi in candidates:
        if pi in matched_predictions or gi in matched_gt:
            continue

        matched_predictions.add(pi)
        matched_gt.add(gi)
        matched_ious.append(iou)

    tp = len(matched_ious)
    fp = len(predictions) - tp
    fn = len(ground_truth) - tp

    return tp, fp, fn, matched_ious


class AccuracyBenchmark(Node):
    def __init__(self):
        super().__init__("accuracy_benchmark")

        print("Loading YOLOv8n...")
        self.model = YOLO(MODEL)

        self.images = OrderedDict()
        self.truth = OrderedDict()

        self.subscriptions_list = []

        self.tp = 0
        self.fp = 0
        self.fn = 0

        self.gt_count = 0
        self.frame_count = 0

        self.matched_ious = []
        self.camera_stats = {
            c: {"frames": 0, "tp": 0, "fp": 0, "fn": 0}
            for c in CAMERAS
        }

        for camera in CAMERAS:
            self.subscriptions_list.append(
                self.create_subscription(
                    Image,
                    f"{IMAGE_PREFIX}/{camera}/image_raw",
                    lambda msg, c=camera: self.on_image(msg, c),
                    10,
                )
            )

            self.subscriptions_list.append(
                self.create_subscription(
                    Detection2DArray,
                    f"{GT_PREFIX}/{camera}",
                    lambda msg, c=camera: self.on_gt(msg, c),
                    10,
                )
            )

        print("Subscribed to synchronized benchmark topics.")

    def on_image(self, msg, camera):
        key = (camera, timestamp(msg))
        self.images[key] = msg
        self.try_match(key)
        self.trim_cache(self.images)

    def on_gt(self, msg, camera):
        key = (camera, timestamp(msg))
        self.truth[key] = msg
        self.try_match(key)
        self.trim_cache(self.truth)

    def trim_cache(self, cache):
        while len(cache) > MAX_CACHE:
            cache.popitem(last=False)

    def try_match(self, key):
        if key not in self.images or key not in self.truth:
            return

        image_msg = self.images.pop(key)
        gt_msg = self.truth.pop(key)

        image = image_to_bgr(image_msg)
        ground_truth = gt_boxes(gt_msg)

        results = self.model.predict(
            source=image,
            conf=CONFIDENCE,
            imgsz=IMAGE_SIZE,
            classes=[0],
            verbose=False,
        )

        predictions = []

        if results and results[0].boxes is not None:
            predictions = (
                results[0]
                .boxes.xyxy.cpu().numpy()
                .astype(np.float32)
                .reshape(-1, 4)
            )

        tp, fp, fn, ious = match_boxes(
            predictions,
            ground_truth,
        )

        self.tp += tp
        self.fp += fp
        self.fn += fn

        self.gt_count += len(ground_truth)
        self.frame_count += 1
        self.matched_ious.extend(ious)

        camera = key[0]
        stats = self.camera_stats[camera]

        stats["frames"] += 1
        stats["tp"] += tp
        stats["fp"] += fp
        stats["fn"] += fn

        if self.frame_count % 30 == 0:
            print(
                f"Processed {self.frame_count} frames | "
                f"GT instances: {self.gt_count}"
            )

    def report(self):
        recall = (
            self.tp / (self.tp + self.fn)
            if self.tp + self.fn
            else 0.0
        )

        fdr = (
            self.fp / (self.tp + self.fp)
            if self.tp + self.fp
            else 0.0
        )

        mean_iou = (
            float(np.mean(self.matched_ious))
            if self.matched_ious
            else 0.0
        )

        print("\n" + "=" * 50)
        print("YOLOv8n ACCURACY BENCHMARK")
        print("=" * 50)

        print(f"Frames evaluated: {self.frame_count}")
        print(f"GT person instances: {self.gt_count}")
        print(f"True positives: {self.tp}")
        print(f"False positives: {self.fp}")
        print(f"False negatives: {self.fn}")

        print(f"\nRecall: {recall * 100:.2f}%")
        print(f"False discovery rate: {fdr * 100:.2f}%")
        print(f"Mean matched IoU: {mean_iou:.3f}")

        print("\nPer-camera results:")

        for camera, stats in self.camera_stats.items():
            total_gt = stats["tp"] + stats["fn"]
            total_pred = stats["tp"] + stats["fp"]

            camera_recall = (
                stats["tp"] / total_gt
                if total_gt else 0.0
            )

            camera_fdr = (
                stats["fp"] / total_pred
                if total_pred else 0.0
            )

            print(
                f"{camera}: "
                f"frames={stats['frames']}, "
                f"recall={camera_recall * 100:.2f}%, "
                f"FDR={camera_fdr * 100:.2f}%"
            )

        enough_data = self.gt_count >= MIN_GT_INSTANCES

        passed = (
            enough_data
            and recall >= MIN_RECALL
            and fdr <= MAX_FDR
        )

        print("\n" + "=" * 50)

        if not enough_data:
            print(
                "INCOMPLETE: fewer than "
                f"{MIN_GT_INSTANCES} GT instances"
            )
        elif passed:
            print("PASS")
        else:
            print("FAIL")

        print("=" * 50)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--seconds",
        type=float,
        default=60,
    )

    args = parser.parse_args()

    rclpy.init()
    node = AccuracyBenchmark()

    print(
        f"Running benchmark for {args.seconds:.0f} seconds..."
    )

    start = time.monotonic()

    try:
        while (
            rclpy.ok()
            and time.monotonic() - start < args.seconds
        ):
            rclpy.spin_once(
                node,
                timeout_sec=0.05,
            )

    except KeyboardInterrupt:
        pass

    finally:
        node.report()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

