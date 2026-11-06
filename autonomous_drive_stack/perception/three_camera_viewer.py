import threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image
from ultralytics import YOLO

names = ['left', 'front', 'right']
frames = {}
latest = None
lock = threading.Lock()

class Cameras(Node):
    def __init__(self):
        super().__init__('three_camera_viewer')
        self.subs = [
            self.create_subscription(
                Image, f'/camera/{name}/image_raw',
                lambda msg, n=name: self.receive(n, msg),
                qos_profile_sensor_data
            ) for name in names
        ]

    def receive(self, name, msg):
        enc = msg.encoding.lower()
        ch = {'rgb8':3, 'bgr8':3, 'rgba8':4, 'bgra8':4}.get(enc)
        if not ch:
            return
        try:
            a = np.frombuffer(msg.data, np.uint8)
            a = a.reshape(msg.height, msg.step)
            a = a[:, :msg.width*ch].reshape(msg.height, msg.width, ch)
            if enc == 'rgb8':
                a = cv2.cvtColor(a, cv2.COLOR_RGB2BGR)
            elif enc == 'rgba8':
                a = cv2.cvtColor(a, cv2.COLOR_RGBA2BGR)
            elif enc == 'bgra8':
                a = cv2.cvtColor(a, cv2.COLOR_BGRA2BGR)
            with lock:
                frames[name] = a.copy()
        except (ValueError, cv2.error):
            pass

def process():
    global latest
    model = YOLO('yolov8n.pt')
    while True:
        tiles = []
        for name in names:
            with lock:
                img = frames.get(name)
            if img is None:
                tile = np.zeros((360,480,3), np.uint8)
            else:
                result = model.predict(img, device=0, imgsz=640,
                                       conf=0.25, verbose=False)[0]
                tile = cv2.resize(result.plot(), (480,360))
            cv2.putText(tile, name.upper(), (15,35),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,255), 2)
            tiles.append(tile)
        ok, jpg = cv2.imencode('.jpg', np.hstack(tiles))
        if ok:
            with lock:
                latest = jpg.tobytes()

class Web(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            data = b'<body style="background:#111;color:white;text-align:center"><h2>PROVE Live YOLO</h2><img src="/stream" width="100%"></body>'
            self.send_response(200)
            self.send_header('Content-Type','text/html')
            self.end_headers()
            self.wfile.write(data)
        elif self.path == '/stream':
            self.send_response(200)
            self.send_header('Content-Type',
                             'multipart/x-mixed-replace; boundary=frame')
            self.end_headers()
            try:
                while True:
                    with lock:
                        jpg = latest
                    if jpg:
                        self.wfile.write(
                            b'--frame\r\nContent-Type: image/jpeg\r\n\r\n'
                            + jpg + b'\r\n'
                        )
                    time.sleep(0.1)
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass

    def log_message(self, *args):
        pass

rclpy.init()
node = Cameras()
threading.Thread(target=rclpy.spin, args=(node,), daemon=True).start()
threading.Thread(target=process, daemon=True).start()
print('Viewer running on port 8765', flush=True)
ThreadingHTTPServer(('0.0.0.0',8765), Web).serve_forever()
