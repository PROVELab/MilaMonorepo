# Justfile - Build, Setup, and Run Commands


### Dashboard Stuff ###
setup_dashboard:
    (cd dashboard && npm install)

setup_reverse_camera:
    (cd reverse-camera && uv sync)

start_dashboard: setup_dashboard
    (cd dashboard && npm run tauri dev)

start_reverse_camera_recv: setup_reverse_camera
    (cd reverse-camera && uv run receiver.py)

