#!/usr/bin/env bash

# MILA localization demonstration launcher.
#
# Usage:
#   ./start_demo.sh
#       Prepare the stage and start sensor nodes + example consumer.
#
#   ./start_demo.sh --no-consumer
#       Prepare the stage and start sensor nodes without the example
#       consumer. Use this when running your own localization program.
#
#   ./start_demo.sh --sensors-only
#       Start sensor nodes + example consumer without preparing the stage.
#       Useful on an Orin receiving data from Isaac Sim on another machine.
#
#   ./start_demo.sh --sensors-only --no-consumer
#       Start only the sensor publisher processes.
#
# Isaac Sim is opened separately. Open the stage printed by this script
# and press Play.

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SIMULATION_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

STAGE_DIR="${SIMULATION_DIR}/isaac_stage"
ASSETS_DIR="${SIMULATION_DIR}/assets"

STAGE_FILE="${STAGE_DIR}/parking_lot_drive_test.usda"
VEHICLE_FILE="${STAGE_DIR}/mila_vehicle.usd"
DRIVE_FILE="${SCRIPT_DIR}/mila_vehicle_drive_graph.py"
SENSOR_LAUNCHER="${SCRIPT_DIR}/start_sensor_demo.sh"

SENSORS_ONLY=false
SENSOR_ARGS=()

usage() {
    cat <<'EOF'
Usage:
  ./start_demo.sh
  ./start_demo.sh --no-consumer
  ./start_demo.sh --sensors-only
  ./start_demo.sh --sensors-only --no-consumer

Options:
  --no-consumer   Do not start the example C++ consumer.
  --sensors-only  Do not prepare or print a local Isaac stage.
  --help          Show this help.

Press Ctrl+C to stop the sensor processes.
EOF
}

for arg in "$@"; do
    case "$arg" in
        --no-consumer)
            SENSOR_ARGS+=("--no-consumer")
            ;;
        --sensors-only)
            SENSORS_ONLY=true
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: ${arg}" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! -x "${SENSOR_LAUNCHER}" ]]; then
    echo "ERROR: Sensor launcher not found or not executable:" >&2
    echo "  ${SENSOR_LAUNCHER}" >&2
    exit 1
fi

if [[ "${SENSORS_ONLY}" == false ]]; then
    for file in "${STAGE_FILE}" "${VEHICLE_FILE}" "${DRIVE_FILE}"; do
        if [[ ! -f "${file}" ]]; then
            echo "ERROR: Required stage file not found:" >&2
            echo "  ${file}" >&2
            exit 1
        fi
    done

    if [[ ! -d "${ASSETS_DIR}" ]]; then
        echo "ERROR: Repository assets folder not found:" >&2
        echo "  ${ASSETS_DIR}" >&2
        exit 1
    fi

    if [[ -n "${WSL_DISTRO_NAME:-}" ]]; then
        # Isaac Sim on Windows needs a Windows-local stage.
        if ! command -v cmd.exe >/dev/null 2>&1; then
            echo "ERROR: WSL cannot find cmd.exe." >&2
            exit 1
        fi

        if ! command -v wslpath >/dev/null 2>&1; then
            echo "ERROR: WSL cannot find wslpath." >&2
            exit 1
        fi

        windows_profile="$(
            cd /mnt/c
            cmd.exe /c 'echo %USERPROFILE%' | tr -d '\r'
        )"

        if [[ -z "${windows_profile}" ]]; then
            echo "ERROR: Could not determine the Windows user profile." >&2
            exit 1
        fi

        windows_profile_wsl="$(wslpath -u "${windows_profile}")"
        documents_dir="${windows_profile_wsl}/Documents"

        if [[ ! -d "${documents_dir}" ]]; then
            echo "ERROR: Windows Documents folder not found:" >&2
            echo "  ${documents_dir}" >&2
            exit 1
        fi

        # Each run gets its own folder so existing stages are not overwritten.
        bundle_dir="$(
            mktemp -d "${documents_dir}/MilaLocalizationDemo.XXXXXXXX"
        )"

        mkdir -p \
            "${bundle_dir}/isaac_stage" \
            "${bundle_dir}/localization"

        cp -- "${STAGE_FILE}" \
            "${bundle_dir}/isaac_stage/parking_lot_drive_test.usda"

        cp -- "${VEHICLE_FILE}" \
            "${bundle_dir}/isaac_stage/mila_vehicle.usd"

        cp -- "${DRIVE_FILE}" \
            "${bundle_dir}/localization/mila_vehicle_drive_graph.py"

        cp -R -- "${ASSETS_DIR}" "${bundle_dir}/assets"

        windows_stage="$(
            wslpath -w \
                "${bundle_dir}/isaac_stage/parking_lot_drive_test.usda"
        )"

        echo
        echo "Prepared a Windows-local stage."
        echo "Open this file in Isaac Sim:"
        echo
        echo "  ${windows_stage}"
        echo
        echo "Windows test files are stored in:"
        echo "  $(wslpath -w "${bundle_dir}")"
        echo

    else
        echo
        echo "Open this stage in Isaac Sim:"
        echo
        echo "  ${STAGE_FILE}"
        echo
    fi

    echo "After the stage opens, press Play to start the drive test."
    echo
else
    echo
    echo "Sensors-only mode: no local stage will be prepared."
    echo "Make sure Isaac Sim is publishing the expected topics."
    echo
fi

# Replace this shell with the existing sensor launcher so Ctrl+C
# reaches its cleanup handling.
exec "${SENSOR_LAUNCHER}" "${SENSOR_ARGS[@]}"
