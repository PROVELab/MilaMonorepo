import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image, CameraInfo
from cv_bridge import CvBridge
import cv2
import yaml
import os


def load_camera_info(yaml_path: str, width: int, height: int) -> CameraInfo:
    info = CameraInfo()
    info.width = width
    info.height = height
    if yaml_path and os.path.isfile(yaml_path):
        with open(yaml_path, 'r') as f:
            calib = yaml.safe_load(f)
        info.width = calib.get('image_width', width)
        info.height = calib.get('image_height', height)
        info.k = calib['camera_matrix']['data']
        info.d = calib['distortion_coefficients']['data']
        info.r = calib['rectification_matrix']['data']
        info.p = calib['projection_matrix']['data']
        info.distortion_model = calib.get('distortion_model', 'plumb_bob')
    return info


class WebcamDriverNode(Node):
    def __init__(self):
        super().__init__('webcam_driver_node')

        self.declare_parameter('device_id', 0)
        self.declare_parameter('video_source', '')
        self.declare_parameter('frame_id', 'camera_optical_frame')
        self.declare_parameter('image_width', 640)
        self.declare_parameter('image_height', 480)
        self.declare_parameter('fps', 30.0)
        self.declare_parameter('camera_info_url', '')
        self.declare_parameter('image_topic', 'image_raw')
        self.declare_parameter('camera_info_topic', 'camera_info')

        device_id = self.get_parameter('device_id').value
        video_source = self.get_parameter('video_source').value
        capture_target = video_source if video_source else device_id
        self.frame_id = self.get_parameter('frame_id').value
        width = self.get_parameter('image_width').value
        height = self.get_parameter('image_height').value
        fps = self.get_parameter('fps').value
        calib_path = self.get_parameter('camera_info_url').value
        image_topic = self.get_parameter('image_topic').value
        info_topic = self.get_parameter('camera_info_topic').value

        self.bridge = CvBridge()
        self.camera_info = load_camera_info(calib_path, width, height)
        if not calib_path or not os.path.isfile(calib_path):
            self.get_logger().warn(
                'No calibration file found — publishing uncalibrated '
                'CameraInfo. Run camera_calibration against this node\'s '
                'image topic, then point camera_info_url at the saved yaml.'
            )

        self.image_pub = self.create_publisher(Image, image_topic, 10)
        self.info_pub = self.create_publisher(CameraInfo, info_topic, 10)

        self.cap = cv2.VideoCapture(capture_target)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)

        if not self.cap.isOpened():
            self.get_logger().error(f'Could not open camera device {device_id}')
            raise RuntimeError(f'Could not open camera device {device_id}')

        timer_period = 1.0 / fps if fps > 0 else 1.0 / 30.0
        self.timer = self.create_timer(timer_period, self.timer_callback)
        self.get_logger().info(
            f'Webcam driver started: device={device_id}, '
            f'publishing {image_topic} + {info_topic}'
        )

    def timer_callback(self):
        ret, frame = self.cap.read()
        if not ret:
            video_source = self.get_parameter('video_source').value
            if video_source:
                self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                ret, frame = self.cap.read()
            if not ret:
                self.get_logger().warn('Failed to read frame from camera')
                return

        now = self.get_clock().now().to_msg()

        img_msg = self.bridge.cv2_to_imgmsg(frame, encoding='bgr8')
        img_msg.header.stamp = now
        img_msg.header.frame_id = self.frame_id

        self.camera_info.header.stamp = now
        self.camera_info.header.frame_id = self.frame_id

        self.image_pub.publish(img_msg)
        self.info_pub.publish(self.camera_info)

    def destroy_node(self):
        self.cap.release()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = WebcamDriverNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()