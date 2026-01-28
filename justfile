# Justfile - Build, Setup, and Run Commands


### Dashboard Stuff ###
setup_dashboard:
    (cd dashboard && npm install)

setup_reverse_camera:
    (cd reverse-camera && uv sync)

build_dashboard: setup_dashboard
    (cd dashboard && npm run tauri:build)

dashboard: setup_dashboard
    (cd dashboard && npm run tauri dev)

reverse_camera_recv: setup_reverse_camera
    (cd reverse-camera && uv run receiver.py)

[parallel]
dashboard_reverse_camera: reverse_camera_recv dashboard

### Build Embedded libs ###
build_embedded:
    (cd mila-embedded && pio run)

### Formatting/Code Quality ###
format:
    (cd mila-embedded && ./format.sh)

check_format:
    (cd mila-embedded && ./check_format.sh)

### Telem Dashboard Stuff ###
telem_dashboard:
    (cd telem-dashboard && gradle run)

