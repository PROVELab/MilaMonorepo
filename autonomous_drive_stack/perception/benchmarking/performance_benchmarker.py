
"""Live YOLO performance benchmark for three Isaac Sim cameras.

Reads ROS 2 camera topics directly.
No images, videos, or predictions are saved.
"""

import argparse
import time
from collections import defaultdict

import numpy as np
import rclpy
import torch
from rclpy.node import Node
from sensor_msgs.msg import Image
from ultralytics import YOLO


CAMERAS = {
    "front": "/camera/front/image_raw",
    "left": "/camera/left/image_raw",
    "right": "/camera/right/image_raw",
}


class PerformanceBenchmark(Node):
    def __init__(self, args):
        super().__init__("performance_benchmark")

        self.args = args
        self.model = YOLO(args.model, task="detect")
        self.cuda = args.device != "cpu" and torch.cuda.is_available()

        self.stats = defaultdict(
            lambda: {"frames": 0, "latency_ms": 0.0}
        )

        self.gpu_samples = []
        self.vram_samples = []
        self.nvml = None

        # Optional NVIDIA GPU monitoring
        try:
            import pynvml
            pynvml.nvmlInit()
            self.nvml = pynvml
            self.gpu = pynvml.nvmlDeviceGetHandleByIndex(0)
        except Exception:
            self.nvml = None

        # Warm up YOLO before measuring
        sample = np.zeros((640, 640, 3), dtype=np.uint8)

        for _ in range(5):
            self.model.predict(
                sample,
                imgsz=args.imgsz,
                device=args.device,
                verbose=False,
            )

        self.sync()
        self.started = time.monotonic()

        self.subscriptions = []

        for name, topic in CAMERAS.items():
            sub = self.create_subscription(
                Image,
                topic,
                lambda msg, camera=name: self.process(msg, camera),
                10,
            )
            self.subscriptions.append(sub)

        self.timer = self.create_timer(5.0, self.report)

        print("\nLive performance benchmark started.")
        print("Cameras: front, left, right")
        print(f"Duration: {args.seconds} seconds")
        print("No images will be saved.\n")

    def sync(self):
        if self.cuda:
            torch.cuda.synchronize()

    def process(self, msg, camera):
        if time.monotonic() - self.started >= self.args.seconds:
            return

        if msg.encoding not in ("rgb8", "bgr8"):
            self.get_logger().warning(
                f"Unsupported image encoding: {msg.encoding}",
                once=True,
            )
            return

        raw = np.frombuffer(msg.data, dtype=np.uint8)
        frame = raw.reshape(msg.height, msg.step)
        frame = frame[:, :msg.width * 3].reshape(
            msg.height, msg.width, 3
        )

        if msg.encoding == "rgb8":
            frame = frame[:, :, ::-1]

        frame = np.ascontiguousarray(frame)

        self.sync()
        start = time.perf_counter()

        self.model.predict(
            frame,
            imgsz=self.args.imgsz,
            conf=self.args.conf,
            device=self.args.device,
            verbose=False,
        )

        self.sync()
        latency = (time.perf_counter() - start) * 1000

        stats = self.stats[camera]
        stats["frames"] += 1
        stats["latency_ms"] += latency

        if self.nvml:
            try:
                util = self.nvml.nvmlDeviceGetUtilizationRates(
                    self.gpu
                ).gpu
                vram = self.nvml.nvmlDeviceGetMemoryInfo(
                    self.gpu
                ).used / (1024 ** 2)

                self.gpu_samples.append(util)
                self.vram_samples.append(vram)
            except Exception:
                pass

    def report(self):
        elapsed = min(
            time.monotonic() - self.started,
            self.args.seconds
        )

        print(f"\n===== PERFORMANCE: {elapsed:.0f}s =====")

        total_frames = 0

        for camera in CAMERAS:
            s = self.stats[camera]
            frames = s["frames"]
            total_frames += frames

            fps = frames / elapsed if elapsed else 0
            latency = (
                s["latency_ms"] / frames
                if frames else 0
            )

            print(
                f"{camera:5s} | "
                f"{fps:5.1f} FPS | "
                f"{latency:6.1f} ms avg latency"
            )

        throughput = total_frames / elapsed if elapsed else 0

        print(f"Total throughput: {throughput:.1f} FPS")

        if self.gpu_samples:
            print(
                f"Average GPU usage: "
                f"{np.mean(self.gpu_samples):.1f}%"
            )
            print(
                f"Average VRAM used: "
                f"{np.mean(self.vram_samples):.0f} MB"
            )

    def cleanup(self):
        if self.nvml:
            self.nvml.nvmlShutdown()


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--model", default="yolov8n.pt")
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--conf", type=float, default=0.25)
    parser.add_argument("--device", default="0")

    args = parser.parse_args()

    rclpy.init()
    node = None

    try:
        node = PerformanceBenchmark(args)

        while (
            rclpy.ok()
            and time.monotonic() - node.started < args.seconds
        ):
            rclpy.spin_once(node, timeout_sec=0.1)

        node.report()

    except KeyboardInterrupt:
        if node:
            node.report()

    finally:
        if node:
            node.cleanup()
            node.destroy_node()

        rclpy.shutdown()


if __name__ == "__main__":
    main()
