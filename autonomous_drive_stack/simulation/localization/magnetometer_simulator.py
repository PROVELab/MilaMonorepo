"""Publish a realistic MMC5983MA-style magnetic field topic from Isaac Sim.

Run in Ubuntu WSL2 after sourcing ROS 2 Humble:
    python3 magnetometer_simulator.py

Input:  /imu       Isaac Sim's clean IMU orientation (used only as simulation truth)
Output: /mag       sensor_msgs/msg/MagneticField in the imu_link frame, in tesla

This publishes a raw-style magnetic-field measurement.  Your ESKF should use
/mag, not Isaac's perfect /imu.orientation, when you later add magnetic heading
updates.
"""

import math
import random
import time

import rclpy
from rclpy.node import Node
from rclpy.executors import ExternalShutdownException
from sensor_msgs.msg import Imu, MagneticField
from sensor_config import DEFAULT_CONFIG, MAG, load_config


# Synthetic Earth magnetic field expressed in Isaac's world frame, in tesla.
# Magnitude: about 47 uT.  This is deliberately a known test field, not yet a
# location-specific World Magnetic Model value.
WORLD_FIELD_T = [20e-6, 8e-6, -42e-6]

# MMC5983MA datasheet: about 0.4 mG RMS = 0.04 uT = 4e-8 T RMS.
WHITE_NOISE_STD_T = 4e-8

# Mild vehicle/mounting distortions.  These are fixed after startup, as they
# generally are for a particular mounted sensor.
HARD_IRON_BIAS_STD_T = 2e-6
SOFT_IRON_SCALE_STD = 0.03
SOFT_IRON_CROSS_AXIS_STD = 0.01
RANDOM_SEED = 17  # Set to None to produce a different test on every run.


def quaternion_xyzw_to_rotation(q):
    """Return the 3x3 sensor-to-world rotation matrix for a ROS [x,y,z,w] q."""
    x, y, z, w = q
    if not all(math.isfinite(v) for v in q):
        raise ValueError('Received non-finite orientation quaternion')
    norm = math.sqrt(x * x + y * y + z * z + w * w)
    if norm < 1e-12:
        raise ValueError("Received a zero-length orientation quaternion")
    x, y, z, w = x / norm, y / norm, z / norm, w / norm
    return [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]


def transpose_times_vector(matrix, vector):
    """Compute matrix.T @ vector without requiring NumPy."""
    return [
        matrix[0][i] * vector[0] + matrix[1][i] * vector[1] + matrix[2][i] * vector[2]
        for i in range(3)
    ]


class MagnetometerSimulator(Node):
    def __init__(self) -> None:
        super().__init__("magnetometer_simulator")
        global WORLD_FIELD_T, WHITE_NOISE_STD_T, HARD_IRON_BIAS_STD_T
        global SOFT_IRON_SCALE_STD, SOFT_IRON_CROSS_AXIS_STD, RANDOM_SEED
        self.declare_parameter('config_file', str(DEFAULT_CONFIG))
        config_path = self.get_parameter('config_file').value
        cfg = load_config(config_path, 'magnetometer', MAG)
        WORLD_FIELD_T = [cfg['world_field_x_t'], cfg['world_field_y_t'], cfg['world_field_z_t']]
        WHITE_NOISE_STD_T = cfg['white_noise_std_t']
        HARD_IRON_BIAS_STD_T = cfg['hard_iron_bias_std_t']
        SOFT_IRON_SCALE_STD = cfg['soft_iron_scale_std']
        SOFT_IRON_CROSS_AXIS_STD = cfg['soft_iron_cross_axis_std']
        RANDOM_SEED = cfg['random_seed']
        self.get_logger().info(f'Loaded startup config {config_path}: {cfg}')
        if RANDOM_SEED is not None:
            random.seed(RANDOM_SEED)

        self.declare_parameter('mag_rate_hz', cfg['mag_rate_hz'])
        self.last_stamp_ns = None
        self.last_log_time = time.monotonic()

        self.publisher = self.create_publisher(MagneticField, "/mag", 10)
        self.subscription = self.create_subscription(Imu, "/imu", self.callback, 10)
        self.hard_iron_bias = [random.gauss(0.0, HARD_IRON_BIAS_STD_T) for _ in range(3)]
        self.soft_iron = self.make_soft_iron_matrix()
        self.messages_since_log = 0
        self.create_timer(2.0, self.log_status)
        self.get_logger().info("Publishing simulated MMC5983MA measurements on /mag")

    @staticmethod
    def make_soft_iron_matrix():
        """Create a near-identity fixed scale/cross-axis distortion matrix."""
        matrix = []
        for row in range(3):
            matrix.append([])
            for col in range(3):
                if row == col:
                    matrix[row].append(1.0 + random.gauss(0.0, SOFT_IRON_SCALE_STD))
                else:
                    matrix[row].append(random.gauss(0.0, SOFT_IRON_CROSS_AXIS_STD))
        return matrix

    def callback(self, imu: Imu) -> None:
        # Isaac's /imu orientation rotates vectors from sensor coordinates into
        # the Isaac world frame.  Rotate the known world magnetic field back
        # into the sensor/imu_link frame to emulate a physical magnetometer.
        if imu.orientation_covariance[0] == -1:
            self.get_logger().warn('IMU orientation unavailable; skipping magnetometer sample')
            return
        stamp_ns = imu.header.stamp.sec * 1_000_000_000 + imu.header.stamp.nanosec
        if self.last_stamp_ns is not None:
            if stamp_ns == self.last_stamp_ns:
                return
            if stamp_ns < self.last_stamp_ns:
                self.get_logger().warn('IMU time moved backward; resetting magnetometer sampling schedule')
                self.last_stamp_ns = None
            else:
                rate = float(self.get_parameter('mag_rate_hz').value)
                if rate > 0 and stamp_ns - self.last_stamp_ns < 1e9 / rate:
                    return
        try:
            sensor_to_world = quaternion_xyzw_to_rotation([
                imu.orientation.x,
                imu.orientation.y,
                imu.orientation.z,
                imu.orientation.w,
            ])
        except ValueError as error:
            self.get_logger().warn(str(error))
            return

        self.last_stamp_ns = stamp_ns

        ideal_sensor_field = transpose_times_vector(sensor_to_world, WORLD_FIELD_T)
        distorted = [
            sum(self.soft_iron[row][col] * ideal_sensor_field[col] for col in range(3))
            + self.hard_iron_bias[row]
            + random.gauss(0.0, WHITE_NOISE_STD_T)
            for row in range(3)
        ]

        out = MagneticField()
        out.header = imu.header
        out.magnetic_field.x, out.magnetic_field.y, out.magnetic_field.z = distorted
        out.magnetic_field_covariance = [
            WHITE_NOISE_STD_T**2, 0.0, 0.0,
            0.0, WHITE_NOISE_STD_T**2, 0.0,
            0.0, 0.0, WHITE_NOISE_STD_T**2,
        ]
        self.publisher.publish(out)
        self.messages_since_log += 1

    def log_status(self) -> None:
        now = time.monotonic()
        elapsed = max(now - self.last_log_time, 1e-9)
        self.last_log_time = now
        self.get_logger().info(
            f"/mag: {self.messages_since_log / elapsed:.1f} Hz wall-clock | "
            f"hard-iron bias={[f'{v * 1e6:.2f}' for v in self.hard_iron_bias]} uT"
        )
        self.messages_since_log = 0


def main() -> None:
    rclpy.init()
    node = MagnetometerSimulator()

    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()

if __name__ == "__main__":
    main()
