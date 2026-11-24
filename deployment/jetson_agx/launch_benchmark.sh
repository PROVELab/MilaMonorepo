#!/bin/bash
set -e
cd "$(dirname "$0")/../.."
source ~/yolo-env/bin/activate
source /opt/ros/jazzy/setup.bash
unset ROS_DISCOVERY_SERVER
export ROS_DOMAIN_ID=0
python autonomous_drive_stack/perception/benchmarking/accuracy_benchmark.py --seconds "${1:-30}"
