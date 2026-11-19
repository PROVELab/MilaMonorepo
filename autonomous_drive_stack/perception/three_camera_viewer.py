
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from collections import OrderedDict

import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image
from vision_msgs.msg import Detection2DArray
from ultralytics import YOLO

names = ["left", "front", "right"]
frames = {}
ground_truth = {}
latest = None
lock = threading.Lock()


def stamp(msg):
    return (msg.header.stamp.sec, msg.header.stamp.nanosec)


class Cameras(Node):
    def __init__(self):
        super().__init__("three_camera_viewer")
        self.subs = []

        for name in names:
            self.subs.append(
                self.create_subscription(
                    Image,
                    f"/camera/{name}/image_raw",
                    lambda msg, n=name: self.receive_image(n, msg),
                    qos_profile_sensor_data
                )
            )

            self.subs.append(
                self.create_subscription(
                    Detection2DArray,
                    f"/ground_truth/{name}",
                    lambda msg, n=name: self.receive_gt(n, msg),
                    10
                )
            )

    def receive_image(self, name, msg):
        enc = msg.encoding.lower()
        ch = {
            "rgb8": 3,
            "bgr8": 3,
            "rgba8": 4,
            "bgra8": 4
        }.get(enc)

        if not ch:
            return

        try:
            a = np.frombuffer(msg.data, np.uint8)
            a = a.reshape(msg.height, msg.step)
            a = a[:, :msg.width * ch]
            a = a.reshape(msg.height, msg.width, ch)

            if enc == "rgb8":
                a = cv2.cvtColor(a, cv2.COLOR_RGB2BGR)
            elif enc == "rgba8":
                a = cv2.cvtColor(a, cv2.COLOR_RGBA2BGR)
            elif enc == "bgra8":
                a = cv2.cvtColor(a, cv2.COLOR_BGRA2BGR)

            with lock:
                frames[name] = (a.copy(), stamp(msg))

        except (ValueError, cv2.error):
            pass

    def receive_gt(self, name, msg):
        boxes = []

        for detection in msg.detections:
            if not detection.results:
                continue

            label = str(
                detection.results[0].hypothesis.class_id
            )

            if label not in ("person", "0"):
                continue

            b = detection.bbox
            x = b.center.position.x
            y = b.center.position.y
            w = b.size_x
            h = b.size_y

            boxes.append((
                int(x - w / 2),
                int(y - h / 2),
                int(x + w / 2),
                int(y + h / 2)
            ))

        with lock:
            ground_truth[name] = (boxes, time.monotonic())


def process():
    global latest

    model = YOLO("yolov8n.pt")

    while True:
        tiles = []

        for name in names:
            with lock:
                item = frames.get(name)
                gt_item = ground_truth.get(name)

                if item is None:
                    img = None
                else:
                    img, image_stamp = item
                    img = img.copy()

                if gt_item is None:
                    boxes = None
                    gt_age = None
                else:
                    boxes, received_at = gt_item
                    boxes = list(boxes)
                    gt_age = time.monotonic() - received_at

            if img is None:
                tile = np.zeros((360, 480, 3), np.uint8)

            else:
                result = model.predict(
                    img,
                    device=0,
                    imgsz=640,
                    conf=0.25,
                    classes=[0],
                    verbose=False
                )[0]

                predictions = result.boxes.xyxy.cpu().numpy()

                # Green: YOLO detections
                for x1, y1, x2, y2 in predictions.astype(int):
                    cv2.rectangle(
                        img,
                        (x1, y1),
                        (x2, y2),
                        (0, 255, 0),
                        3
                    )

                # Red: Isaac Sim ground truth
                if boxes is not None:
                    for x1, y1, x2, y2 in boxes:
                        cv2.rectangle(
                            img,
                            (x1, y1),
                            (x2, y2),
                            (0, 0, 255),
                            3
                        )

                gt_text = (
                    str(len(boxes))
                    if boxes is not None
                    else "NO DATA"
                )

                cv2.putText(
                    img,
                    f"YOLO: {len(predictions)} | GT: {gt_text}",
                    (20, 75),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    1.1,
                    (255, 255, 255),
                    3
                )

                if gt_age is not None and gt_age > 2:
                    cv2.putText(
                        img,
                        f"GT STALE: {gt_age:.1f}s",
                        (20, 120),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.9,
                        (0, 0, 255),
                        2
                    )

                tile = cv2.resize(img, (480, 360))

            cv2.putText(
                tile,
                name.upper(),
                (15, 35),
                cv2.FONT_HERSHEY_SIMPLEX,
                1,
                (0, 255, 255),
                2
            )

            tiles.append(tile)

        ok, jpg = cv2.imencode(".jpg", np.hstack(tiles))

        if ok:
            with lock:
                latest = jpg.tobytes()


class Web(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            data = (
                b'<body style="background:#111;color:white;'
                b'text-align:center">'
                b'<h2>PROVE YOLO vs Ground Truth</h2>'
                b'<p>Green = YOLO | Red = Isaac Sim</p>'
                b'<img src="/stream" width="100%">'
                b'</body>'
            )

            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(data)

        elif self.path == "/stream":
            self.send_response(200)
            self.send_header(
                "Content-Type",
                "multipart/x-mixed-replace; boundary=frame"
            )
            self.end_headers()

            try:
                while True:
                    with lock:
                        jpg = latest

                    if jpg:
                        self.wfile.write(
                            b"--frame\r\n"
                            b"Content-Type: image/jpeg\r\n\r\n"
                            + jpg + b"\r\n"
                        )

                    time.sleep(0.1)

            except (
                BrokenPipeError,
                ConnectionResetError,
                OSError
            ):
                pass

    def log_message(self, *args):
        pass


rclpy.init()
node = Cameras()

threading.Thread(
    target=rclpy.spin,
    args=(node,),
    daemon=True
).start()

threading.Thread(
    target=process,
    daemon=True
).start()

print("Viewer running on port 8765", flush=True)

ThreadingHTTPServer(
    ("0.0.0.0", 8765),
    Web
).serve_forever()

