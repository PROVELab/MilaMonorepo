#!/usr/bin/env python3
"""Forward selected ROS 2 messages over one TCP connection.

RTX 5070: python3 ros2_tcp_bridge.py send
Jetson:   python3 ros2_tcp_bridge.py receive
"""
import argparse
import cv2
import numpy as np
import queue
import socket
import struct
import threading

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from rclpy.serialization import serialize_message, deserialize_message
from sensor_msgs.msg import Image
from vision_msgs.msg import Detection2DArray

PORT = 9000
# (source topic on RTX 5070, destination topic on Jetson, ROS type)
TOPICS = [
    (f'/benchmark_camera/{cam}/image_raw', f'/camera/{cam}/image_raw', Image)
    for cam in ('front', 'left', 'right')
] + [
    (f'/ground_truth/{cam}', f'/ground_truth/{cam}', Detection2DArray)
    for cam in ('front', 'left', 'right')
]
# Protocol: 1-byte topic index, 4-byte network-order payload length, serialized ROS message.
MAX_MESSAGE = 32 * 1024 * 1024


def read_exact(sock, count):
    parts = bytearray()
    while len(parts) < count:
        chunk = sock.recv(count - len(parts))
        if not chunk:
            raise ConnectionError('Connection closed')
        parts.extend(chunk)
    return bytes(parts)


def send_mode():
    rclpy.init()
    node = Node('isaac_tcp_sender')
    outgoing = queue.Queue(maxsize=12)
    dropped = [0]

    def make_callback(index):
        def callback(message):
            if isinstance(message, Image):
                if message.encoding != "rgb8":
                    return
                frame = np.frombuffer(
                    message.data, dtype=np.uint8
                ).reshape(message.height, message.step)
                frame = frame[:, :message.width * 3].reshape(
                    message.height, message.width, 3
                )
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                ok, encoded = cv2.imencode(
                    ".jpg", frame,
                    [cv2.IMWRITE_JPEG_QUALITY, 85]
                )
                if not ok:
                    return
                message.data = encoded.tobytes()
                message.encoding = "jpeg"

            packet = serialize_message(message)
            if len(packet) > MAX_MESSAGE:
                return
            try:
                outgoing.put_nowait((index, packet))
            except queue.Full:
                dropped[0] += 1  # Never block ROS camera callbacks.
        return callback

    subscriptions = [
        node.create_subscription(msg_type, src, make_callback(i), qos_profile_sensor_data)
        for i, (src, _, msg_type) in enumerate(TOPICS)
    ]
    threading.Thread(target=rclpy.spin, args=(node,), daemon=True).start()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(('100.99.16.110', PORT))
        server.listen(1)
        print(f'RTX 5070: waiting for Jetson on 100.99.16.110:{PORT}', flush=True)
        while True:
            conn, _ = server.accept()
            print('Jetson connected; forwarding live ROS messages', flush=True)
            with conn:
                try:
                    while True:
                        index, payload = outgoing.get()
                        conn.sendall(struct.pack('!BI', index, len(payload)) + payload)
                except (BrokenPipeError, ConnectionError, OSError):
                    print('Disconnected; waiting for reconnection', flush=True)
                    while not outgoing.empty():
                        try:
                            outgoing.get_nowait()
                        except queue.Empty:
                            break


def receive_mode():
    rclpy.init()
    node = Node('jetson_tcp_receiver')
    publishers = [
        node.create_publisher(msg_type, dst, qos_profile_sensor_data)
        for _, dst, msg_type in TOPICS
    ]
    while rclpy.ok():
        try:
            print(f'Connecting to RTX 5070 at 100.99.16.110:{PORT} ...', flush=True)
            with socket.create_connection(('100.99.16.110', PORT), timeout=10) as sock:
                sock.settimeout(None)
                print('Connected. Republishing ROS 2 topics on Jetson.', flush=True)
                while True:
                    index, size = struct.unpack('!BI', read_exact(sock, 5))
                    if index >= len(TOPICS) or size > MAX_MESSAGE:
                        raise ValueError('Invalid bridge packet')
                    payload = read_exact(sock, size)
                    msg = deserialize_message(payload, TOPICS[index][2])
                    if isinstance(msg, Image) and msg.encoding == "jpeg":
                        frame = cv2.imdecode(
                            np.frombuffer(msg.data, dtype=np.uint8),
                            cv2.IMREAD_COLOR
                        )
                        if frame is None:
                            continue
                        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                        msg.height, msg.width = frame.shape[:2]
                        msg.encoding = "rgb8"
                        msg.step = msg.width * 3
                        msg.data = frame.tobytes()

                    publishers[index].publish(msg)
        except (OSError, ConnectionError, ValueError) as exc:
            print(f'Bridge disconnected: {exc}. Retry in 3 seconds.', flush=True)
            threading.Event().wait(3)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['send', 'receive'])
    args = parser.parse_args()
    try:
        send_mode() if args.mode == 'send' else receive_mode()
    except KeyboardInterrupt:
        pass
