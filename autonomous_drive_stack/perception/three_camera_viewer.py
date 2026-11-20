import threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image
from vision_msgs.msg import Detection2DArray
from collections import OrderedDict

names = ['left', 'front', 'right']
frames = {}
detections = {}
latest = None
lock = threading.Lock()
image_buffers = {name: OrderedDict() for name in names}
detection_buffers = {name: OrderedDict() for name in names}

def stamp_key(stamp):
    return (stamp.sec, stamp.nanosec)

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
        #subscribe to Yolo detection topic
        self.det_subs = [
            self.create_subscription(
                Detection2DArray, f'/detections/{name}',
                lambda msg, n=name: self.receive_detections(n, msg),
                qos_profile_sensor_data
            ) for name in names
        ]
    
    #receive bounding boxes instead of running yolo again 
    def receive_detections(self, name, msg):
        boxes = []

        for det in msg.detections:
            cx = det.bbox.center.position.x
            cy = det.bbox.center.position.y
            w = det.bbox.size_x
            h = det.bbox.size_y

            x1 = cx - w / 2
            y1 = cy - h / 2
            x2 = cx + w / 2
            y2 = cy + h / 2

            label = "object"
            score = 0.0

            if det.results:
                label = str(det.results[0].hypothesis.class_id)
                score = float(det.results[0].hypothesis.score)

            boxes.append((x1, y1, x2, y2, label, score))

        with lock:
            key = stamp_key(msg.header.stamp)
            detection_buffers[name][key] = boxes

            if len(detection_buffers[name]) > 30:
                detection_buffers[name].popitem(last=False)

            match_frames(name, key)

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
                key = stamp_key(msg.header.stamp)
                image_buffers[name][key] = a.copy()

                if len(image_buffers[name]) > 30:
                    image_buffers[name].popitem(last=False)

                match_frames(name, key)
        except (ValueError, cv2.error):
            pass
def match_frames(name, key):
    # Called while lock is held
    if key not in image_buffers[name]:
        return

    if key not in detection_buffers[name]:
        return

    frames[name] = image_buffers[name].pop(key)
    detections[name] = detection_buffers[name].pop(key)
    
def process():
    global latest
    
    while True:
        tiles = []
        for name in names:
            with lock:
                img = frames.get(name)
                boxes = detections.get(name, [])
            if img is None:
                tile = np.zeros((360,480,3), np.uint8)
            else:
                #Resize original image, not YOLO result
                h, w = img.shape[:2]
                tile = cv2.resize(img, (480,360))

                #Draw boxes received from detector_node
                for x1, y1, x2, y2, label, score in boxes:
                    pt1 = (int(x1 * 480 / w), int(y1 * 360 / h))
                    pt2 = (int(x2 * 480 / w), int(y2 * 360 / h))

                    cv2.rectangle(tile, pt1, pt2, (0,255,0), 2)
                    cv2.putText(
                        tile, f'{label} {score:.2f}',
                        (pt1[0], max(15, pt1[1] - 5)),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, (0,255,0), 1
                    )

            cv2.putText(tile, name.upper(), (15,35),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,255), 2)
            tiles.append(tile)

        ok, jpg = cv2.imencode('.jpg', np.hstack(tiles))
        if ok:
            with lock:
                latest = jpg.tobytes()

        time.sleep(0.1)  #Avoid unnecessary CPU usage

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
