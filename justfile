# Justfile - Build, Setup, and Run Commands

### Autogen ###
autogen:
    @command -v protoc >/dev/null || (echo "Missing protoc (Protocol Buffers compiler)." && exit 1)
    @command -v protoc-gen-nanopb >/dev/null || (echo "Missing protoc-gen-nanopb. Install with: python3 -m pip install --user nanopb protobuf" && exit 1)
    # Generate into generated/ (makes it easy to test)
    (cd autogen/vsr && cargo run -- generated)
    (cd autogen/vsr && protoc -I generated --plugin=protoc-gen-nanopb=$(command -v protoc-gen-nanopb) --nanopb_out=generated generated/vsr.proto)
    # Generate into the actual vsr spot
    (cd autogen/vsr && cargo run -- ../../mila-embedded/src/mcu/vsr)
    (cd autogen/vsr && protoc -I ../../mila-embedded/src/mcu/vsr --plugin=protoc-gen-nanopb=$(command -v protoc-gen-nanopb) --nanopb_out=../../mila-embedded/src/mcu/vsr ../../mila-embedded/src/mcu/vsr/vsr.proto)

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

# Build one target
build_embedded: autogen
    @read -p "PIO target: " pio_env; \
    (cd mila-embedded && pio run -e $pio_env)

# Builds all pio envs in mila-embedded
build_embedded_all: autogen
    (cd mila-embedded && pio run)

# Generates compile_commands.json for a specific (prompt)
# platformio environment
generate_cc_db:
    @read -p "Just target: " pio_env; \
    (cd mila-embedded && pio run -t compiledb -e $pio_env)

### Formatting/Code Quality ###
format:
    (cd mila-embedded && ./format.sh)
    cargo fmt --all

check_format:
    (cd mila-embedded && ./check_format.sh)
    cargo fmt --all --check

### Telem Dashboard Stuff ###
telem_dashboard:
    (cd telem-dashboard && gradle run)
