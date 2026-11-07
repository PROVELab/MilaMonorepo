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
import time

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
from sensor_config import DEFAULT_CONFIG, IMU, load_config


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
        global ACCEL_NOISE_DENSITY_MPS2_PER_SQRT_HZ, GYRO_NOISE_DENSITY_RADPS_PER_SQRT_HZ
        global ACCEL_INITIAL_BIAS_STD_MPS2, GYRO_INITIAL_BIAS_STD_RADPS
        global ACCEL_BIAS_RANDOM_WALK_MPS2_PER_SQRT_S, GYRO_BIAS_RANDOM_WALK_RADPS_PER_SQRT_S
        self.declare_parameter('config_file', str(DEFAULT_CONFIG))
        config_path = self.get_parameter('config_file').value
        cfg = load_config(config_path, 'imu', IMU)
        ACCEL_NOISE_DENSITY_MPS2_PER_SQRT_HZ = cfg['accel_noise_density_mps2_per_sqrt_hz']
        GYRO_NOISE_DENSITY_RADPS_PER_SQRT_HZ = cfg['gyro_noise_density_radps_per_sqrt_hz']
        ACCEL_INITIAL_BIAS_STD_MPS2 = cfg['accel_initial_bias_std_mps2']
        GYRO_INITIAL_BIAS_STD_RADPS = cfg['gyro_initial_bias_std_radps']
        ACCEL_BIAS_RANDOM_WALK_MPS2_PER_SQRT_S = cfg['accel_bias_random_walk_mps2_per_sqrt_s']
        GYRO_BIAS_RANDOM_WALK_RADPS_PER_SQRT_S = cfg['gyro_bias_random_walk_radps_per_sqrt_s']
        self.get_logger().info(f'Loaded startup config {config_path}: {cfg}')
        self.publisher = self.create_publisher(Imu, "/imu_noisy", 10)
        self.subscription = self.create_subscription(Imu, "/imu", self.callback, 10)

        self.declare_parameter('seed', cfg['seed'])
        self.declare_parameter('nominal_rate_hz', cfg['nominal_rate_hz'])
        self.declare_parameter('max_gap_seconds', cfg['max_gap_seconds'])
        self.declare_parameter('reset_bias_on_time_reset', cfg['reset_bias_on_time_reset'])
        self.rng = random.Random(int(self.get_parameter('seed').value))

        self.accel_bias = [
            self.rng.gauss(0.0, ACCEL_INITIAL_BIAS_STD_MPS2) for _ in range(3)
        ]
        self.gyro_bias = [
            self.rng.gauss(0.0, GYRO_INITIAL_BIAS_STD_RADPS) for _ in range(3)
        ]
        self.previous_time = None
        self.messages_since_log = 0
        self.last_log_time = time.monotonic()
        self.create_timer(2.0, self.log_status)

        self.get_logger().info("Publishing noisy IMU data on /imu_noisy")

    @staticmethod
    def stamp_to_seconds(msg: Imu) -> float:
        return msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9

    def callback(self, msg: Imu) -> None:
        timestamp = self.stamp_to_seconds(msg)
        nominal_rate = float(self.get_parameter('nominal_rate_hz').value)
        if nominal_rate <= 0:
            self.get_logger().error('nominal_rate_hz must be positive')
            return
        if self.previous_time is None:
            dt = 1.0 / nominal_rate
        else:
            dt = timestamp - self.previous_time
            if dt == 0:
                self.get_logger().warn('Duplicate IMU timestamp; skipping sample')
                return
            if dt < 0:
                self.get_logger().warn('IMU clock moved backward; starting new epoch')
                if self.get_parameter('reset_bias_on_time_reset').value:
                    self.rng.seed(int(self.get_parameter('seed').value))
                    self.accel_bias = [self.rng.gauss(0.0, ACCEL_INITIAL_BIAS_STD_MPS2) for _ in range(3)]
                    self.gyro_bias = [self.rng.gauss(0.0, GYRO_INITIAL_BIAS_STD_RADPS) for _ in range(3)]
                dt = 1.0 / nominal_rate
            elif dt > float(self.get_parameter('max_gap_seconds').value):
                self.get_logger().warn(f'IMU timestamp gap: {dt:.3f} s; evolving bias over gap')
        self.previous_time = timestamp

        sqrt_dt = math.sqrt(dt)
        # White-noise density N becomes per-sample sigma N*sqrt(BW), where
        # BW is approximately sample_rate/2 = 1/(2*dt).
        # Fixed nominal bandwidth: packet gaps do not change the sensor filter.
        accel_sigma = ACCEL_NOISE_DENSITY_MPS2_PER_SQRT_HZ * math.sqrt(nominal_rate / 2.0)
        gyro_sigma = GYRO_NOISE_DENSITY_RADPS_PER_SQRT_HZ * math.sqrt(nominal_rate / 2.0)

        for axis in range(3):
            self.accel_bias[axis] += self.rng.gauss(
                0.0, ACCEL_BIAS_RANDOM_WALK_MPS2_PER_SQRT_S * sqrt_dt
            )
            self.gyro_bias[axis] += self.rng.gauss(
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
            accel_clean[i] + self.accel_bias[i] + self.rng.gauss(0.0, accel_sigma)
            for i in range(3)
        ]
        gyro_noisy = [
            gyro_clean[i] + self.gyro_bias[i] + self.rng.gauss(0.0, gyro_sigma)
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
        now = time.monotonic()
        elapsed = max(now - self.last_log_time, 1e-9)
        self.last_log_time = now
        self.get_logger().info(
            f"/imu_noisy: {self.messages_since_log / elapsed:.1f} Hz wall-clock | "
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
