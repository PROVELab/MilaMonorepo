# Justfile - Build, Setup, and Run Commands

### Autogen ###
autogen:
    (cd autogen/vsr/ && cargo run -- ../../mila-embedded/src/mcu/)

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

### Build Embedded stuff ###
# Builds all pio envs in mila-embedded
build_embedded: autogen
    (cd mila-embedded && pio run)

# Generates compile_commands.json for a specific (prompt)
# platformio environment
generate_cc_db:
    @read -p "Just target: " pio_env; \
    (cd mila-embedded && pio run -t compiledb -e $pio_env)

### Formatting/Code Quality ###
format:
    (cd mila-embedded && ./format.sh)

check_format:
    (cd mila-embedded && ./check_format.sh)

### Telem Dashboard Stuff ###
telem_dashboard:
    (cd telem-dashboard && gradle run)

