#!/bin/bash
set -e
cd /opt/isaacsim
unset ROS_DISCOVERY_SERVER
export ROS_DOMAIN_ID=0
export ROS_DISTRO=jazzy
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export LD_LIBRARY_PATH=/opt/isaacsim/exts/isaacsim.ros2.core/jazzy/lib:$LD_LIBRARY_PATH
export PYTHONPATH=/opt/isaacsim/exts/isaacsim.ros2.core/jazzy/rclpy:$PYTHONPATH

./isaac-sim.streaming.sh \
 --/exts/omni.kit.livestream.app/primaryStream/publicIp="$(tailscale ip -4)" \
 --/exts/omni.kit.livestream.app/primaryStream/signalPort=49000 \
 --/exts/omni.kit.livestream.app/primaryStream/streamPort=47800 \
 --enable isaacsim.ros2.bridge
