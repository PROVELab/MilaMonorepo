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

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu, MagneticField


@dataclass
class ImuSample:
    stamp_ns: int
    frame_id: str
    accel_mps2: tuple[float, float, float]
    gyro_radps: tuple[float, float, float]


@dataclass
class MagSample:
    stamp_ns: int
    frame_id: str
    field_tesla: tuple[float, float, float]


def stamp_to_ns(stamp) -> int:
    return stamp.sec * 1_000_000_000 + stamp.nanosec


class SensorInputNode(Node):
    def __init__(self) -> None:
        super().__init__("sensor_input_node")
        self.latest_imu: Optional[ImuSample] = None
        self.latest_mag: Optional[MagSample] = None
        self.last_report_ns = 0
        self.warned_frame_mismatch = False

        self.create_subscription(Imu, "/imu_noisy", self.imu_callback, 50)
        self.create_subscription(MagneticField, "/mag", self.mag_callback, 50)
        self.create_timer(1.0, self.report_status)

        self.get_logger().info("Waiting for /imu_noisy and /mag ...")

    def imu_callback(self, msg: Imu) -> None:
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
        )

    def mag_callback(self, msg: MagneticField) -> None:
        self.latest_mag = MagSample(
            stamp_ns=stamp_to_ns(msg.header.stamp),
            frame_id=msg.header.frame_id,
            field_tesla=(
                msg.magnetic_field.x,
                msg.magnetic_field.y,
                msg.magnetic_field.z,
            ),
        )

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

        if imu.frame_id != mag.frame_id:
            self.get_logger().error(
                f"Frame mismatch: IMU='{imu.frame_id}', magnetometer='{mag.frame_id}'. "
                "They must describe the same imu_link frame."
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
