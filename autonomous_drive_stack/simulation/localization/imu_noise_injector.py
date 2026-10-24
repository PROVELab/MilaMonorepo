"""Add ISM330DHCX-like accelerometer and gyroscope errors to Isaac Sim /imu.

Run in Ubuntu WSL2 after sourcing ROS 2 Humble:
    python3 imu_noise_injector.py

Input:  /imu          (clean Isaac Sim IMU)
Output: /imu_noisy    (accel/gyro noise + bias drift)

The orientation on /imu_noisy is deliberately marked unavailable.  Isaac's
orientation is simulator ground truth, whereas the real SparkFun board supplies
raw accel, gyro, and magnetometer data; the ESKF/AHRS must estimate orientation.
"""

import math
import random

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu


# ISM330DHCX high-performance values from its datasheet.
ACCEL_NOISE_DENSITY_MPS2_PER_SQRT_HZ = 60e-6 * 9.80665  # 60 ug/sqrt(Hz)
GYRO_NOISE_DENSITY_RADPS_PER_SQRT_HZ = math.radians(8e-3)  # 8 mdps/sqrt(Hz)

# Deliberately conservative starting estimates. Tune these after recording data
# from the actual board during a stationary test.
ACCEL_INITIAL_BIAS_STD_MPS2 = 0.03
GYRO_INITIAL_BIAS_STD_RADPS = 0.005
ACCEL_BIAS_RANDOM_WALK_MPS2_PER_SQRT_S = 2e-4
GYRO_BIAS_RANDOM_WALK_RADPS_PER_SQRT_S = 2e-5


class ImuNoiseInjector(Node):
    def __init__(self) -> None:
        super().__init__("imu_noise_injector")
        self.publisher = self.create_publisher(Imu, "/imu_noisy", 10)
        self.subscription = self.create_subscription(Imu, "/imu", self.callback, 10)

        self.accel_bias = [
            random.gauss(0.0, ACCEL_INITIAL_BIAS_STD_MPS2) for _ in range(3)
        ]
        self.gyro_bias = [
            random.gauss(0.0, GYRO_INITIAL_BIAS_STD_RADPS) for _ in range(3)
        ]
        self.previous_time = None
        self.messages_since_log = 0
        self.create_timer(2.0, self.log_status)

        self.get_logger().info("Publishing noisy IMU data on /imu_noisy")

    @staticmethod
    def stamp_to_seconds(msg: Imu) -> float:
        return msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9

    def callback(self, msg: Imu) -> None:
        timestamp = self.stamp_to_seconds(msg)
        if self.previous_time is None:
            dt = 1.0 / 100.0
        else:
            dt = timestamp - self.previous_time
            # Protect the noise model from stale, repeated, or very delayed stamps.
            if not 0.0001 <= dt <= 1.0:
                dt = 1.0 / 100.0
        self.previous_time = timestamp

        sqrt_dt = math.sqrt(dt)
        # White-noise density N becomes per-sample sigma N*sqrt(BW), where
        # BW is approximately sample_rate/2 = 1/(2*dt).
        accel_sigma = ACCEL_NOISE_DENSITY_MPS2_PER_SQRT_HZ / math.sqrt(2.0 * dt)
        gyro_sigma = GYRO_NOISE_DENSITY_RADPS_PER_SQRT_HZ / math.sqrt(2.0 * dt)

        for axis in range(3):
            self.accel_bias[axis] += random.gauss(
                0.0, ACCEL_BIAS_RANDOM_WALK_MPS2_PER_SQRT_S * sqrt_dt
            )
            self.gyro_bias[axis] += random.gauss(
                0.0, GYRO_BIAS_RANDOM_WALK_RADPS_PER_SQRT_S * sqrt_dt
            )

        accel_clean = [
            msg.linear_acceleration.x,
            msg.linear_acceleration.y,
            msg.linear_acceleration.z,
        ]
        gyro_clean = [
            msg.angular_velocity.x,
            msg.angular_velocity.y,
            msg.angular_velocity.z,
        ]
        accel_noisy = [
            accel_clean[i] + self.accel_bias[i] + random.gauss(0.0, accel_sigma)
            for i in range(3)
        ]
        gyro_noisy = [
            gyro_clean[i] + self.gyro_bias[i] + random.gauss(0.0, gyro_sigma)
            for i in range(3)
        ]

        out = Imu()
        out.header = msg.header
        out.linear_acceleration.x, out.linear_acceleration.y, out.linear_acceleration.z = accel_noisy
        out.angular_velocity.x, out.angular_velocity.y, out.angular_velocity.z = gyro_noisy

        # Do not leak Isaac's perfect orientation into the noisy sensor stream.
        out.orientation.w = 1.0
        out.orientation_covariance = [-1.0] + [0.0] * 8
        out.linear_acceleration_covariance = [
            accel_sigma**2, 0.0, 0.0,
            0.0, accel_sigma**2, 0.0,
            0.0, 0.0, accel_sigma**2,
        ]
        out.angular_velocity_covariance = [
            gyro_sigma**2, 0.0, 0.0,
            0.0, gyro_sigma**2, 0.0,
            0.0, 0.0, gyro_sigma**2,
        ]

        self.publisher.publish(out)
        self.messages_since_log += 1

    def log_status(self) -> None:
        self.get_logger().info(
            f"/imu_noisy: {self.messages_since_log / 2.0:.1f} Hz | "
            f"accel bias={['%.4f' % v for v in self.accel_bias]} m/s^2 | "
            f"gyro bias={['%.5f' % v for v in self.gyro_bias]} rad/s"
        )
        self.messages_since_log = 0


def main() -> None:
    rclpy.init()
    node = ImuNoiseInjector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
