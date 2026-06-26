# Justfile - Build, Setup, and Run Commands

### Autogen ###
ensure_motor_h300:
    @if [ -e mila-embedded/src/mcu/motor_h300 ] && [ ! -L mila-embedded/src/mcu/motor_h300 ]; then \
        echo "mila-embedded/src/mcu/motor_h300 exists and is not a symlink; refusing to replace it."; \
        exit 1; \
    elif [ -d ../motor_h300/firmware ]; then \
        ln -sfn ../../../../motor_h300/firmware mila-embedded/src/mcu/motor_h300; \
    elif [ -d motor_h300/firmware ]; then \
        ln -sfn ../../../motor_h300/firmware mila-embedded/src/mcu/motor_h300; \
    else \
        echo "Could not find motor_h300 checkout. Expected ../motor_h300 or ./motor_h300 relative to MilaMonorepo."; \
        exit 1; \
    fi

autogen: ensure_motor_h300
    @command -v protoc >/dev/null || (echo "Missing protoc (Protocol Buffers compiler)." && exit 1)
    @command -v protoc-gen-nanopb >/dev/null || (echo "Missing protoc-gen-nanopb. Install nanopb for your active Python environment." && exit 1)
    # Generate into generated/ (makes it easy to test)
    (cd autogen/vsr && cargo run -- generated); \
    (cd autogen/vsr && protoc -I generated --plugin=protoc-gen-nanopb="$(command -v protoc-gen-nanopb)" --nanopb_opt=-f,generated/vsr.options --nanopb_out=generated generated/vsr.proto); \
    # Generate into the actual vsr spot
    (cd autogen/vsr && cargo run -- ../../mila-embedded/src/mcu/vsr); \
    (cd autogen/vsr && protoc -I ../../mila-embedded/src/mcu/vsr --plugin=protoc-gen-nanopb="$(command -v protoc-gen-nanopb)" --nanopb_opt=-f,../../mila-embedded/src/mcu/vsr/vsr.options --nanopb_out=../../mila-embedded/src/mcu/vsr ../../mila-embedded/src/mcu/vsr/vsr.proto)


### Dashboard Stuff ###
setup_dashboard:
    (cd dashboard && npm ci)

setup_reverse_camera:
    (cd reverse-camera && uv sync)

build_dashboard: setup_dashboard
    (cd dashboard && npm run tauri:build)

release_dashboard: release_dashboard_linux

release_dashboard_linux:
    (cd dashboard && npm ci)
    (cd dashboard && npm run lint)
    (cd dashboard && npm run tauri:build -- --ci --bundles deb,rpm)

ci_dashboard: release_dashboard_linux

dashboard: setup_dashboard autogen
    (cd dashboard && npm run tauri dev)

reverse_camera_recv: setup_reverse_camera
    (cd reverse-camera && uv run receiver.py)

[parallel]
dashboard_reverse_camera: reverse_camera_recv dashboard

### Build Embedded stuff ###

# Build one target
build_embedded target='': autogen
    @pio_env="{{target}}"; \
    if [ -z "$pio_env" ]; then read -p "PIO target: " pio_env; fi; \
    (cd mila-embedded && pio run -e $pio_env)

# Flash one target
flash_embedded target='': autogen
    @pio_env="{{target}}"; \
    if [ -z "$pio_env" ]; then read -p "PIO target: " pio_env; fi; \
    (cd mila-embedded && pio run -t upload -e $pio_env)

# Builds all pio envs in mila-embedded
build_embedded_all: autogen
    (cd mila-embedded && pio run)

build_mcu_sitl:
    BUILD=mila-embedded/.pio/build/mcu; \
    esptool --chip esp32 merge-bin \
      --flash-mode dio \
      --flash-freq 40m \
      --flash-size 4MB \
      --pad-to-size 4MB \
      -o "$BUILD/qemu_flash.bin" \
      0x1000 "$BUILD/bootloader.bin" \
      0x8000 "$BUILD/partitions.bin" \
      0x10000 "$BUILD/firmware.bin"

run_mcu_sitl:
    BUILD=mila-embedded/.pio/build/mcu; \
    ~/.espressif/tools/qemu-xtensa/esp_develop_9.2.2_20250817/qemu/bin/qemu-system-xtensa \
      -machine esp32 \
      -nographic \
      -no-reboot \
      -drive file="$BUILD/qemu_flash.bin",if=mtd,format=raw \
      -serial unix:/tmp/mcu_serial.sock,server,nowait \
      -monitor stdio \
      -object can-bus,id=canbus0 \
      -global driver=esp32.twai,property=canbus,value=canbus0 \
      -object can-host-socketcan,id=canhost0,if=vcan0,canbus=canbus0
# socat -d -d PTY,link=/tmp/mcu_serial,raw,echo=0 UNIX-CONNECT:/tmp/mcu_serial.sock 
# Wireshark:
# tshark -i vcan0 -w can_capture.pcapng

ci_embedded: build_embedded_all

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

clean:
    git clean -fdX
