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


### Telem Dashboard Stuff ###
telem_dashboard:
    (cd telem-dashboard && gradle run)

