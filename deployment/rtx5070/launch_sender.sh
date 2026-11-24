#!/bin/bash
set -e
cd "$(dirname "$0")/../.."
unset ROS_DISCOVERY_SERVER
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export LD_LIBRARY_PATH=/opt/isaacsim/exts/isaacsim.ros2.core/jazzy/lib:$LD_LIBRARY_PATH
export PYTHONPATH=/opt/isaacsim/exts/isaacsim.ros2.core/jazzy/rclpy:$PYTHONPATH
/opt/isaacsim/python.sh autonomous_drive_stack/networking/ros2_tcp_bridge.py send
