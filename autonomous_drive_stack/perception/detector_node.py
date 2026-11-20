#!/usr/bin/env python3

"""
detector_node.py
----------------
Per-camera 2D object detector for Jetson AGX Orin.

Subscribes to three Isaac Sim ROS 2 camera topics and publishes
vision_msgs/Detection2DArray messages for sensor fusion.

Runs YOLOv8n using CUDA when available.
"""

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Dict

import torch
import numpy as np
import rclpy

from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy

from sensor_msgs.msg import Image
from vision_msgs.msg import (
    BoundingBox2D,
    Detection2D,
    Detection2DArray,
    ObjectHypothesisWithPose,
)

_YOLO = None


@dataclass
class CameraCfg:
    name: str
    image_topic: str
    frame_id: str


class DetectorNode(Node):

    def __init__(self):
        super().__init__('prove_detector')

        # ---- Parameters ----

        self.declare_parameter('weights', 'yolov8n.pt')
        self.declare_parameter('conf_threshold', 0.35)
        self.declare_parameter('iou_threshold', 0.5)
        self.declare_parameter('input_size', 640)
        self.declare_parameter(
            'device',
            'cuda:0' if torch.cuda.is_available() else 'cpu'
        )
        self.declare_parameter('frame_skip', 1)
        self.declare_parameter('use_tensorrt', False)

        self.declare_parameter(
            'cameras.front.image_topic',
            '/camera/front/image_raw'
        )
        self.declare_parameter(
            'cameras.front.frame_id',
            'cam_front_optical'
        )
        self.declare_parameter(
            'cameras.left.image_topic',
            '/camera/left/image_raw'
        )
        self.declare_parameter(
            'cameras.left.frame_id',
            'cam_left_optical'
        )
        self.declare_parameter(
            'cameras.right.image_topic',
            '/camera/right/image_raw'
        )
        self.declare_parameter(
            'cameras.right.frame_id',
            'cam_right_optical'
        )

        weights = self.get_parameter('weights').value
        self.conf = float(
            self.get_parameter('conf_threshold').value
        )
        self.iou = float(
            self.get_parameter('iou_threshold').value
        )
        self.imgsz = int(
            self.get_parameter('input_size').value
        )
        self.device = self.get_parameter('device').value
        self.frame_skip = max(
            1, int(self.get_parameter('frame_skip').value)
        )
        self.use_trt = bool(
            self.get_parameter('use_tensorrt').value
        )

        # ---- YOLO model ----

        global _YOLO

        if _YOLO is None:
            from ultralytics import YOLO as _Y
            _YOLO = _Y

        self.get_logger().info(
            f'Loading detector weights: {weights}'
        )
        self.get_logger().info(
            f'Inference device: {self.device}'
        )

        self.model = _YOLO(weights)

        if self.use_trt:
            self.get_logger().info(
                'TensorRT backend assumed; weights should be .engine'
            )

        # Warm up inference
        try:
            self.model.predict(
                np.zeros(
                    (self.imgsz, self.imgsz, 3),
                    dtype=np.uint8
                ),
                verbose=False,
                device=self.device,
                imgsz=self.imgsz,
            )
            self.get_logger().info('Model warmed up')

        except Exception as e:
            self.get_logger().warn(
                f'Warmup failed (non-fatal): {e}'
            )

        # ---- ROS 2 IO ----

        self.qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )

        self.cameras: Dict[str, CameraCfg] = {}
        self.frame_count: Dict[str, int] = {}
        self.publishers_ = {}

        # Keep subscription references alive
        self.subscriptions_ = []

        for name in ('front', 'left', 'right'):

            topic = self.get_parameter(
                f'cameras.{name}.image_topic'
            ).value

            frame = self.get_parameter(
                f'cameras.{name}.frame_id'
            ).value

            cfg = CameraCfg(
                name=name,
                image_topic=topic,
                frame_id=frame,
            )

            self.cameras[name] = cfg
            self.frame_count[name] = 0

            det_topic = f'/detections/{name}'

            self.publishers_[name] = self.create_publisher(
                Detection2DArray,
                det_topic,
                self.qos,
            )

            # Store the subscription
            sub = self.create_subscription(
                Image,
                topic,
                lambda msg, n=name: self._on_image(n, msg),
                self.qos,
            )

            self.subscriptions_.append(sub)

            self.get_logger().info(
                f'[{name}] subscribed {topic} -> {det_topic}'
            )

        # ---- Diagnostics ----

        self._last_log = time.time()
        self._det_counts = {
            name: 0 for name in self.cameras
        }

        self.create_timer(5.0, self._log_stats)

    # ---- Image callback ----

    def _on_image(self, cam_name: str, msg: Image) -> None:

        self.frame_count[cam_name] += 1

        if self.frame_count[cam_name] % self.frame_skip != 0:
            return

        try:
            channels = {
                'rgb8': 3,
                'bgr8': 3,
                'rgba8': 4,
                'bgra8': 4,
            }

            if msg.encoding not in channels:
                raise ValueError(
                    f'Unsupported encoding: {msg.encoding}'
                )

            ch = channels[msg.encoding]

            raw = np.frombuffer(
                msg.data,
                dtype=np.uint8
            )

            rows = raw.reshape(
                msg.height,
                msg.step
            )

            cv_img = rows[
                :, :msg.width * ch
            ].reshape(
                msg.height,
                msg.width,
                ch
            ).copy()

            if msg.encoding == 'rgb8':
                cv_img = cv_img[:, :, ::-1].copy()

            elif msg.encoding == 'rgba8':
                cv_img = cv_img[:, :, [2, 1, 0]].copy()

            elif msg.encoding == 'bgra8':
                cv_img = cv_img[:, :, :3].copy()

        except Exception as e:
            self.get_logger().error(
                f'[{cam_name}] image conversion failed: {e}'
            )
            return

        # ---- GPU inference ----

        try:
            results = self.model.predict(
                cv_img,
                verbose=False,
                conf=self.conf,
                iou=self.iou,
                imgsz=self.imgsz,
                device=self.device,
            )

        except Exception as e:
            self.get_logger().error(
                f'[{cam_name}] inference failed: {e}'
            )
            return

        # ---- Publish detections ----

        det_arr = self._results_to_msg(
            results,
            msg,
            cam_name
        )

        self.publishers_[cam_name].publish(det_arr)

        self._det_counts[cam_name] += len(
            det_arr.detections
        )

    # ---- Convert YOLO results to ROS 2 ----

    def _results_to_msg(
        self,
        results,
        img_msg: Image,
        cam_name: str
    ) -> Detection2DArray:

        out = Detection2DArray()
        out.header = img_msg.header

        if not results:
            return out

        r = results[0]

        if r.boxes is None or len(r.boxes) == 0:
            return out

        boxes = r.boxes.xyxy.cpu().numpy()
        clss = r.boxes.cls.cpu().numpy().astype(int)
        confs = r.boxes.conf.cpu().numpy()
        names = r.names

        for (x1, y1, x2, y2), c, p in zip(
            boxes, clss, confs
        ):

            d = Detection2D()
            d.header = out.header

            bb = BoundingBox2D()

            bb.center.position.x = float(
                (x1 + x2) / 2.0
            )
            bb.center.position.y = float(
                (y1 + y2) / 2.0
            )
            bb.center.theta = 0.0

            bb.size_x = float(x2 - x1)
            bb.size_y = float(y2 - y1)

            d.bbox = bb

            hyp = ObjectHypothesisWithPose()

            hyp.hypothesis.class_id = str(
                names.get(int(c), str(c))
            )
            hyp.hypothesis.score = float(p)

            d.results.append(hyp)
            out.detections.append(d)

        return out

    # ---- Performance diagnostics ----

    def _log_stats(self) -> None:

        now = time.time()
        dt = now - self._last_log

        if dt <= 0:
            return

        parts = []

        for name in self.cameras:

            fps_in = self.frame_count[name] / dt
            det_per_s = self._det_counts[name] / dt

            parts.append(
                f'{name}: {fps_in:.1f} fps in, '
                f'{det_per_s:.1f} det/s'
            )

            self.frame_count[name] = 0
            self._det_counts[name] = 0

        self._last_log = now

        self.get_logger().info(
            '[detector] ' + ' | '.join(parts)
        )


def main(args=None):

    rclpy.init(args=args)
    node = DetectorNode()

    try:
        rclpy.spin(node)

    except KeyboardInterrupt:
        pass

    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
