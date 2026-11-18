# Localization

Works out where the car is by fusing the IMU with GPS/RTK, wheel speed (inverter
rpm) and steering angle in an error-state Kalman filter.

## Files

```
localization/
├── CMakeLists.txt
├── include/loc/          what each part offers (C++ headers)
│   ├── config.hpp
│   ├── eskf.hpp
│   ├── localizer.hpp
│   └── live_localizer.hpp
├── src/                  how each part works (matching .cpp files)
│   ├── eskf.cpp
│   ├── localizer.cpp
│   └── live_localizer.cpp
├── tests/
│   └── test_localizer.cpp
└── tools/
    ├── replay.cpp
    ├── sim_drive.py
    ├── evaluate.py
    ├── rosbag_to_replay.py
    └── evaluate_bag.py
```

In C++ a header (`.hpp`) declares what exists: classes, functions, settings.
The `.cpp` file with the same name implements it. Other code only includes the
headers, e.g. `#include "loc/localizer.hpp"`.

| File | What it is |
|---|---|
| `CMakeLists.txt` | Build script. Needs Eigen and GeographicLib (`brew install cmake eigen geographiclib`); downloads manif and doctest itself. Builds the filter library (double and float versions), the replay programs and the tests: `cmake -S . -B build && cmake --build build` |
| `config.hpp` | Every setting in one place. At the top, on/off switches for each sensor (`LOC_USE_*`) and number precision (`LOC_SCALAR`). Below, noise levels, where things are mounted on the car, vehicle size and fault limits. Values marked PLACEHOLDER must be measured on the car. |
| `eskf.hpp`, `eskf.cpp` | The Kalman filter maths, nothing car-specific: move the estimate forward with each IMU sample, correct it with a measurement (skipping outliers), and fold the correction in. |
| `localizer.hpp`, `localizer.cpp` | The car-specific layer: what each sensor should read, start-up (level the IMU, then find the heading), faults that should stop the car, and the output (position, lat/lon/alt, the error it believes it has). Needs readings in time order. |
| `live_localizer.hpp`, `live_localizer.cpp` | What the car calls. On the car, readings arrive late and out of order (GPS ~50-100 ms late), so this holds them for 0.15 s, passes them to the localizer in time order, and predicts the output forward to "now". |
| `test_localizer.cpp` | Safety tests on a tiny simulated car: IMU stops, late data, wheel slip, bad GPS, clearing faults, start-up while turning or reversing. Run: `ctest --test-dir build --output-on-failure` |
| `replay.cpp` | Runs logged sensor data (CSV files) through the filter and writes its estimates (`_est.csv`) and every innovation (`_innov.csv`). Built three ways: `replay` (the car's settings), `replay_f32` (float, like the ESP32) and `replay_all` (every sensor compiled in, for comparisons). |
| `sim_drive.py` | Makes a fake 150 s drive with exact ground truth and every sensor. `--stress` adds GPS outages and bad fixes; `--realistic` adds errors the filter doesn't model (timing, mounting, tyre slip). |
| `evaluate.py` | Runs `sim_drive.py` and `replay` for each sensor combination and compares against the truth. Writes tables and plots to `results/`. Run: `python3 tools/evaluate.py` |
| `rosbag_to_replay.py` | Converts an Isaac Sim recording (`.db3`) into replay CSVs, adding synthetic GPS because the sim has none yet. |
| `evaluate_bag.py` | Scores an Isaac Sim recording in one command: `python3 -I tools/evaluate_bag.py BAG.db3 --name NAME` |

## Isaac Sim recording (2026-11-14)

![Isaac Sim error: IMU only vs GPS only vs fused](results/mila_20261114_error.png)

Horizontal error over an 18 s, 25 m drive in Isaac Sim. The recording has no GPS,
so GPS was made from the sim's ground truth plus RTK-like noise: this is a
best case. IMU + GPS: 1.6 cm RMS with GPS at 8 Hz, 3.8 cm at 1 Hz. IMU alone
drifts to tens of metres, and GPS alone is only right at the moment of each fix.
Made with `tools/evaluate_bag.py`.
