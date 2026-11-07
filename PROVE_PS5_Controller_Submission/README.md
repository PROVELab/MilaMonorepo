# PROVE PS5 Controller Bench Test

Author: Joaquin Bejarano  
Project: Drive-by-wire physical video-game-like controller  
Date: October 2026

## Purpose

This project checks whether a Sony DualSense PS5 controller can be read from a Windows laptop running Ubuntu through WSL2. The Python program converts the controller inputs into simulated steering, throttle, and brake percentages.

This is a bench test. It does not connect to the vehicle, transmit commands to the vehicle compute unit, or control actuators.

## Verified setup

- Windows 11 laptop
- WSL2 with Ubuntu
- DualSense controller connected by USB
- `usbipd-win` used to pass the USB device into WSL2
- Ubuntu detected the controller as `Sony Corp. DualSense wireless controller (PS5)`
- Ubuntu created `/dev/input/js0`

## Input mapping

| Physical control | Linux input | Program function |
|---|---:|---|
| Left stick horizontal | Axis 0 | Steering |
| Left trigger | Axis 3 | Brake |
| Right trigger | Axis 4 | Throttle |
| PlayStation button | Button 12 | Enable/disable |
| Touchpad click | Button 13 | Latched emergency request |

The measured joystick axis range was approximately -32767 to +32767. Trigger readings moved from approximately -32767 released, through 0 near the midpoint, to +32767 fully pressed.

## Run the program

The controller must be attached to WSL2 and appear as `/dev/input/js0`.

From this folder in Ubuntu, run:

```bash
python3 ps5_controller_bench.py
```

Press `Ctrl+C` to stop.

## Bench behavior

- The controller starts disabled.
- The PlayStation button toggles enable when the controls are released.
- Steering uses a center dead zone, a curved response, and a rate limit.
- Throttle rises gradually and can fall immediately.
- Brake remains direct and has priority over throttle.
- Touchpad click latches an emergency state, disables normal outputs, and requires released controls followed by a PlayStation-button press to re-enable.
- All outputs are printed locally in the terminal.

## Dependencies

The controller test used Ubuntu packages including `usbutils` and `joystick`. The Python program uses only Python standard-library modules.

```bash
sudo apt update
sudo apt install usbutils joystick -y
```

## Limitations and next steps

The program has not been tested on the vehicle compute unit and does not implement a vehicle communication protocol. Before integration, the team must confirm the compute unit operating environment, USB support, command format, update timing, command limits, communication-loss behavior, and vehicle-side safety authority. The smoothing values in this bench program are demonstration settings and require validation before any vehicle use.
