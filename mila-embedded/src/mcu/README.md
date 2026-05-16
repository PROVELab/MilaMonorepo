# MCU - The heart
The MCU is at the nexus of all Low voltage activity
in Mila, and as such needs to be a little more complex
than just reading sensor data and sending it over CAN.

## Submodule
To run the motor controller code, you need to also clone the submodule.
It lives at the repo root as `motor_h300/`; firmware-facing C sources live
under `motor_h300/firmware/`. The `src/mcu/motor_h300/` path is kept as
symlinks for the existing MCU build and include paths.
The easiest way to do this (if you have already cloned this repo) is to run
```
git submodule sync --recursive && git submodule update --init --recursive motor_h300
```

If you make changes to the motor code in the subrepo, make sure to commit them,
ideally in a separate branch (but this may not always be possible).
You can use vscode's git view to make this easier.

## Notes before running
- Verify motor controller can timeout ms and baud

## VSR architecture
- The MCU manages state via the VSR (Vehicle Status Register).
- VSR schema lives in `src/mcu/vsr/vsr.proto`.
- Generated files are in `src/mcu/vsr/`:
    - `vsr.pb.h/.c` (nanopb wire structs + encode/decode metadata)
    - `vsr_state.h/.c` (runtime wrapper, per-slot mutexes/timestamps, serialize helper)
- The global runtime state is `volatile vehicle_status_reg_t vsr_global`.
- Each top-level VSR slot has its own mutex and `*_updated_at_us` timestamp.
- Use `ACQ_REL_VSRSEM_R/W` macros from `vsr_state.h` for slot-scoped reads/writes.
- `vsr_serialize(...)` creates a wire-compatible snapshot for logging/transport.
- Treat generated files as owned by codegen: edit the source schema/generator, not the generated C.

## Tasks

- The esp32 has 2 cores running at 240 Mhz each
- Current startup path (`src/mcu/main.c`) initializes `vsr_global`, registers CAN listeners,
  starts pedal sensor flow (`pedal_main`), and starts motor send task (`start_send_motor_task`).
- Console and SD logging tasks are present but currently optional/integration-in-progress.
