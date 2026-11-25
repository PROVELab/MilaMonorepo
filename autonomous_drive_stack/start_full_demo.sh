#!/usr/bin/env bash

# Start the localization and perception processes.
# Isaac Sim and the integrated stage are opened separately.

set -Eeuo pipefail

STACK_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

LOCALIZATION_LAUNCHER="${STACK_DIR}/simulation/localization/start_demo.sh"
PERCEPTION_DIR="${STACK_DIR}/perception"
PERCEPTION_LAUNCHER="${PERCEPTION_DIR}/launch_perception.py"
ROS_SETUP="/opt/ros/humble/setup.bash"

if [[ ! -f "${ROS_SETUP}" ]]; then
    echo "ERROR: ROS 2 Humble not found: ${ROS_SETUP}" >&2
    exit 1
fi

if [[ ! -f "${LOCALIZATION_LAUNCHER}" ]]; then
    echo "ERROR: Localization launcher not found: ${LOCALIZATION_LAUNCHER}" >&2
    exit 1
fi

if [[ ! -f "${PERCEPTION_LAUNCHER}" ]]; then
    echo "ERROR: Perception launcher not found: ${PERCEPTION_LAUNCHER}" >&2
    exit 1
fi

# ROS setup scripts can reference unset variables.
set +u
source "${ROS_SETUP}"
set -u

children=()

cleanup() {
    trap - EXIT INT TERM

    echo
    echo "Stopping demo processes..."

    for pid in "${children[@]}"; do
        kill -TERM "${pid}" 2>/dev/null || true
    done

    for pid in "${children[@]}"; do
        wait "${pid}" 2>/dev/null || true
    done

    echo "Demo stopped."
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "Starting localization sensors..."
bash "${LOCALIZATION_LAUNCHER}" --sensors-only &
children+=("$!")

echo "Starting perception..."
(
    cd "${PERCEPTION_DIR}"
    exec python3 "${PERCEPTION_LAUNCHER}"
) &
children+=("$!")

echo
echo "Open the integrated Map.usda stage in Isaac Sim and press Play."
echo "Open the viewer URL printed by the perception launcher."
echo "Press Ctrl+C here to stop the demo processes."
echo

# If either launcher exits, stop the other.
set +e
wait -n "${children[@]}"
exit_code=$?
set -e

echo "A demo process exited with status ${exit_code}."
exit "${exit_code}"
