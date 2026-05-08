# Justfile - Build, Setup, and Run Commands

### Autogen ###
autogen:
    @command -v protoc >/dev/null || (echo "Missing protoc (Protocol Buffers compiler)." && exit 1)
    # @test -n "${PROTOC_GEN_NANOPB:-}" || command -v protoc-gen-nanopb >/dev/null || (echo "Missing protoc-gen-nanopb. Install with: python3 -m pip install --user nanopb protobuf" && exit 1)
    # Generate into generated/ (makes it easy to test)
    (cd autogen/vsr && cargo run -- generated)
    (cd autogen/vsr && protoc -I generated --plugin=protoc-gen-nanopb="../../deps/generator/protoc-gen-nanopb" --nanopb_opt=-f,generated/vsr.options --nanopb_out=generated generated/vsr.proto)
    # Generate into the actual vsr spot
    (cd autogen/vsr && cargo run -- ../../mila-embedded/src/mcu/vsr)
    (cd autogen/vsr && protoc -I ../../mila-embedded/src/mcu/vsr --plugin="../../deps/generator/protoc-gen-nanopb" --nanopb_opt=-f,../../mila-embedded/src/mcu/vsr/vsr.options --nanopb_out=../../mila-embedded/src/mcu/vsr ../../mila-embedded/src/mcu/vsr/vsr.proto)


### Dashboard Stuff ###
setup_dashboard:
    (cd dashboard && npm install)

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
      -serial pty \
      -monitor stdio

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
