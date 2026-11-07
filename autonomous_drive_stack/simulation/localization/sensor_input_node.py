#!/usr/bin/env python3
"""Read the simulated noisy IMU and magnetometer as one sensor-input stream.

Run in WSL after Isaac Sim is publishing /imu and the two simulator nodes are
running:
    source /opt/ros/humble/setup.bash
    python3 sensor_input_node.py

Inputs:
    /imu_noisy  sensor_msgs/msg/Imu
    /mag        sensor_msgs/msg/MagneticField

This is deliberately only an input/validation node.  It does not estimate
orientation or position yet; the next ESKF node will consume these same raw
measurements.
"""

from dataclasses import dataclass
from typing import Optional
from collections import deque
import time

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu, MagneticField


@dataclass
class ImuSample:
    stamp_ns: int
    frame_id: str
    accel_mps2: tuple[float, float, float]
    gyro_radps: tuple[float, float, float]
    accel_covariance: tuple[float, ...] = ()
    gyro_covariance: tuple[float, ...] = ()


@dataclass
class MagSample:
    stamp_ns: int
    frame_id: str
    field_tesla: tuple[float, float, float]
    covariance: tuple[float, ...] = ()


def stamp_to_ns(stamp) -> int:
    return stamp.sec * 1_000_000_000 + stamp.nanosec


class SensorInputNode(Node):
    def __init__(self) -> None:
        super().__init__("sensor_input_node")
        self.latest_imu: Optional[ImuSample] = None
        self.latest_mag: Optional[MagSample] = None
        self.last_report_ns = 0
        self.warned_frame_mismatch = False
        self.declare_parameter('stale_seconds', 2.0)
        self.declare_parameter('imu_queue_capacity', 1000)
        capacity = int(self.get_parameter('imu_queue_capacity').value)
        if capacity <= 0:
            raise ValueError('imu_queue_capacity must be positive')
        self.imu_queue = deque(maxlen=capacity)
        self.imu_queue_overflows = 0
        self.imu_received_at = None
        self.mag_received_at = None

        self.create_subscription(Imu, "/imu_noisy", self.imu_callback, 50)
        self.create_subscription(MagneticField, "/mag", self.mag_callback, 50)
        self.create_timer(1.0, self.report_status)

        self.get_logger().info("Waiting for /imu_noisy and /mag ...")

    def imu_callback(self, msg: Imu) -> None:
        stamp = stamp_to_ns(msg.header.stamp)
        if self.latest_imu and stamp <= self.latest_imu.stamp_ns:
            if stamp == self.latest_imu.stamp_ns:
                self.get_logger().warn('Duplicate IMU timestamp; skipping sample')
                return
            self.get_logger().warn('IMU time moved backward; clearing old sensor state and queue')
            self.imu_queue.clear()
            self.latest_imu = self.latest_mag = None
            self.imu_received_at = self.mag_received_at = None
        self.latest_imu = ImuSample(
            stamp_ns=stamp_to_ns(msg.header.stamp),
            frame_id=msg.header.frame_id,
            accel_mps2=(
                msg.linear_acceleration.x,
                msg.linear_acceleration.y,
                msg.linear_acceleration.z,
            ),
            gyro_radps=(
                msg.angular_velocity.x,
                msg.angular_velocity.y,
                msg.angular_velocity.z,
            ),
            accel_covariance=tuple(msg.linear_acceleration_covariance),
            gyro_covariance=tuple(msg.angular_velocity_covariance),
        )
        self.imu_received_at = time.monotonic()
        if len(self.imu_queue) == self.imu_queue.maxlen:
            self.imu_queue_overflows += 1
            if self.imu_queue_overflows == 1:
                self.get_logger().warn('IMU queue full: oldest samples are being dropped; drain it or increase capacity')
        self.imu_queue.append(self.latest_imu)

    def drain_imu_samples(self):
        """Drain samples in timestamp order; use with the single-threaded executor."""
        samples = list(self.imu_queue)
        self.imu_queue.clear()
        return samples

    def mag_callback(self, msg: MagneticField) -> None:
        stamp = stamp_to_ns(msg.header.stamp)
        if self.latest_mag and stamp <= self.latest_mag.stamp_ns:
            if stamp == self.latest_mag.stamp_ns:
                self.get_logger().warn('Duplicate magnetometer timestamp; skipping sample')
                return
            self.get_logger().warn('Magnetometer time moved backward; clearing old IMU and magnetometer state')
            self.imu_queue.clear()
            self.latest_imu = self.latest_mag = None
            self.imu_received_at = self.mag_received_at = None
        self.latest_mag = MagSample(
            stamp_ns=stamp_to_ns(msg.header.stamp),
            frame_id=msg.header.frame_id,
            field_tesla=(
                msg.magnetic_field.x,
                msg.magnetic_field.y,
                msg.magnetic_field.z,
            ),
            covariance=tuple(msg.magnetic_field_covariance),
        )
        self.mag_received_at = time.monotonic()

    def report_status(self) -> None:
        imu, mag = self.latest_imu, self.latest_mag
        if imu is None or mag is None:
            missing = []
            if imu is None:
                missing.append("/imu_noisy")
            if mag is None:
                missing.append("/mag")
            self.get_logger().warn("Still waiting for: " + ", ".join(missing))
            return

        timeout = float(self.get_parameter('stale_seconds').value)
        now = time.monotonic()
        stale = [name for name, received in [('/imu_noisy', self.imu_received_at), ('/mag', self.mag_received_at)]
                 if received is None or now - received > timeout]
        if stale:
            self.get_logger().warn('Stale data (wall-clock): ' + ', '.join(stale))
            return

        if imu.frame_id != mag.frame_id:
            self.get_logger().error(
                f"Frame mismatch: IMU='{imu.frame_id}', magnetometer='{mag.frame_id}'. "
                "This monitor expects matching frames; a known transform is required otherwise."
            )
            self.warned_frame_mismatch = True
            return

        delta_ms = abs(imu.stamp_ns - mag.stamp_ns) / 1e6
        ax, ay, az = imu.accel_mps2
        gx, gy, gz = imu.gyro_radps
        mx, my, mz = mag.field_tesla
        magnitude_ut = (mx * mx + my * my + mz * mz) ** 0.5 * 1e6

        self.get_logger().info(
            f"frame={imu.frame_id} | IMU accel=[{ax:+.3f}, {ay:+.3f}, {az:+.3f}] m/s^2 "
            f"gyro=[{gx:+.4f}, {gy:+.4f}, {gz:+.4f}] rad/s | "
            f"MAG=[{mx * 1e6:+.2f}, {my * 1e6:+.2f}, {mz * 1e6:+.2f}] uT "
            f"|B|={magnitude_ut:.2f} uT | timestamp separation={delta_ms:.1f} ms"
        )


def main() -> None:
    rclpy.init()
    node = SensorInputNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
