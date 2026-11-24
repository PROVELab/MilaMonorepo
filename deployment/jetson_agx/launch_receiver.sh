#!/bin/bash
set -e
cd "$(dirname "$0")/../.."
unset PYTHONPATH LD_LIBRARY_PATH ROS_DISCOVERY_SERVER
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=0
python3 autonomous_drive_stack/networking/ros2_tcp_bridge.py receive
