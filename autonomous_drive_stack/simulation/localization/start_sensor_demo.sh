#!/usr/bin/env bash

# Start the localization sensor pipeline.
#
# Supported use:
#   - Linux workstation running Isaac Sim
#   - WSL connected to Isaac Sim running on Windows
#   - Linux/Orin receiving ROS 2 data from a separate Isaac workstation
#
# Prerequisites:
#   - ROS 2 Humble installed
#   - colcon installed
#   - Required ROS 2 and Python dependencies installed
#   - Isaac Sim publishing the expected /imu topic
#
# Usage:
#   ./start_sensor_demo.sh
#   ./start_sensor_demo.sh --no-consumer
#
# The default starts the example C++ consumer for demonstration.
# Use --no-consumer if another program will use the interface instead.

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

ROS_SETUP="/opt/ros/humble/setup.bash"
WORKSPACE_SETUP="${SCRIPT_DIR}/install/setup.bash"

START_CONSUMER=true

usage() {
    cat <<'EOF'
Usage:
  ./start_sensor_demo.sh                Start sensors and example consumer
  ./start_sensor_demo.sh --no-consumer  Start sensor publishers only
  ./start_sensor_demo.sh --help         Show this help

Start Isaac Sim separately, open the provided MILA stage, and press Play.
Press Ctrl+C in this terminal to stop the processes started here.
EOF
}

for arg in "$@"; do
    case "$arg" in
        --no-consumer)
            START_CONSUMER=false
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: $arg" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! -f "${ROS_SETUP}" ]]; then
    echo "ERROR: ROS 2 Humble setup not found: ${ROS_SETUP}" >&2
    echo "Install ROS 2 Humble before running this launcher." >&2
    exit 1
fi

if ! command -v colcon >/dev/null 2>&1; then
    echo "ERROR: colcon was not found in PATH." >&2
    echo "Install colcon before running this launcher." >&2
    exit 1
fi

required_paths=(
    "${SCRIPT_DIR}/imu_noise_injector.py"
    "${SCRIPT_DIR}/magnetometer_simulator.py"
    "${SCRIPT_DIR}/sensor_config.py"
    "${SCRIPT_DIR}/sensor_config.xml"
    "${SCRIPT_DIR}/ros2_sensor_interface"
    "${SCRIPT_DIR}/sensor_consumer_example"
)

for path in "${required_paths[@]}"; do
    if [[ ! -e "${path}" ]]; then
        echo "ERROR: Required file or directory not found: ${path}" >&2
        exit 1
    fi
done

# ROS setup files may refer to unset environment variables.
set +u
source "${ROS_SETUP}"
set -u

cd "${SCRIPT_DIR}"

echo "Building the C++ sensor interface and example consumer..."

colcon build \
    --base-paths ros2_sensor_interface sensor_consumer_example \
    --packages-up-to sensor_consumer_example

if [[ ! -f "${WORKSPACE_SETUP}" ]]; then
    echo "ERROR: Build completed, but workspace setup was not found:" >&2
    echo "  ${WORKSPACE_SETUP}" >&2
    exit 1
fi

set +u
source "${WORKSPACE_SETUP}"
set -u

children=()

cleanup() {
    trap - INT TERM EXIT

    echo
    echo "Stopping sensor demo processes..."

    for pid in "${children[@]}"; do
        kill -TERM "${pid}" 2>/dev/null || true
    done

    for pid in "${children[@]}"; do
        wait "${pid}" 2>/dev/null || true
    done

    echo "Sensor demo stopped."
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "Starting IMU noise injector..."
python3 -u imu_noise_injector.py &
children+=("$!")

echo "Starting magnetometer simulator..."
python3 -u magnetometer_simulator.py &
children+=("$!")

if [[ "${START_CONSUMER}" == true ]]; then
    echo "Starting example C++ consumer..."
    ros2 run sensor_consumer_example consumer &
    children+=("$!")
else
    echo "Example C++ consumer disabled."
fi

echo
echo "Sensor processes started."
echo "Open the MILA stage in Isaac Sim and press Play."
echo "The drive test should start when the stage plays."
echo "Press Ctrl+C here to stop the sensor processes."
echo

# If any child exits unexpectedly, shut down the remaining children.
set +e
wait -n "${children[@]}"
exit_code=$?
set -e

echo "A sensor process exited with status ${exit_code}."
exit "${exit_code}"