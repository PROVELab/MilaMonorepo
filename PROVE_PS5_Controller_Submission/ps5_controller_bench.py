"""PS5 controller bench test for Ubuntu/WSL2. Does not connect to a vehicle."""

import os
import select
import struct
import time


DEVICE = "/dev/input/js0"

# Numbers measured with jstest on this DualSense in this Ubuntu setup.
STEERING_AXIS = 0
BRAKE_AXIS = 3
THROTTLE_AXIS = 4
ENABLE_BUTTON = 12        # PlayStation logo button
EMERGENCY_BUTTON = 13     # Touchpad click

STICK_DEAD_ZONE = 0.07
STEERING_RATE = 100.0      # Percentage points per second (bench setting)
THROTTLE_RISE_RATE = 50.0 # Percentage points per second (bench setting)
PRINT_INTERVAL = 0.10
POLL_INTERVAL = 0.02

# Linux joystick events contain: milliseconds, value, type, number.
EVENT = struct.Struct("<IhBB")
BUTTON_EVENT = 0x01
AXIS_EVENT = 0x02
INITIAL_EVENT = 0x80


def clamp(value, low, high):
    return max(low, min(high, value))


def trigger_percent(raw):
    # Released is -32767 and fully pressed is +32767 on this controller.
    return clamp((raw + 32767) * 100.0 / 65534, 0.0, 100.0)


def steering_percent(raw):
    position = clamp(raw / 32767.0, -1.0, 1.0)
    amount = abs(position)
    if amount <= STICK_DEAD_ZONE:
        return 0.0

    # The curve makes small stick movements less sensitive.
    amount = (amount - STICK_DEAD_ZONE) / (1.0 - STICK_DEAD_ZONE)
    return (1 if position > 0 else -1) * amount * amount * 100.0


def move_toward(current, target, maximum_change):
    return current + clamp(target - current, -maximum_change, maximum_change)


def neutral_for_enable(axes):
    # Require the steering stick and both triggers to be released before arming.
    return (abs(axes[STEERING_AXIS]) <= 2500
            and trigger_percent(axes[THROTTLE_AXIS]) <= 5.0
            and trigger_percent(axes[BRAKE_AXIS]) <= 5.0)


def main():
    axes = {STEERING_AXIS: 0, BRAKE_AXIS: -32767, THROTTLE_AXIS: -32767}
    seen_axes = set()
    enabled = False
    emergency_latched = False
    enable_held = False
    emergency_held = False
    steering = 0.0
    throttle = 0.0
    last_step = time.monotonic()
    last_print = 0.0

    print("PS5 bench test: NO vehicle communication or actuator output.")
    print("PS button: enable/disable. Touchpad click: latch emergency.")
    print("After an emergency, release the touchpad and all controls, then press PS.")
    print("Press Ctrl+C to stop.\n")

    try:
        joystick = os.open(DEVICE, os.O_RDONLY | os.O_NONBLOCK)
    except OSError as error:
        print(f"Cannot open {DEVICE}: {error}")
        print("Check that the controller is attached to WSL and /dev/input/js0 exists.")
        return

    try:
        while True:
            readable, _, _ = select.select([joystick], [], [], POLL_INTERVAL)
            if readable:
                # Drain all queued changes so button presses and releases are not lost.
                while True:
                    try:
                        data = os.read(joystick, EVENT.size)
                    except BlockingIOError:
                        break
                    if len(data) != EVENT.size:
                        raise OSError("Controller disconnected or input stream ended")

                    _, value, event_type, number = EVENT.unpack(data)
                    event_type &= ~INITIAL_EVENT

                    if event_type == AXIS_EVENT and number in axes:
                        axes[number] = value
                        seen_axes.add(number)

                    elif event_type == BUTTON_EVENT and number == EMERGENCY_BUTTON:
                        emergency_held = bool(value)
                        if emergency_held:
                            emergency_latched = True
                            enabled = False
                            steering = 0.0
                            throttle = 0.0
                            print("\nEMERGENCY LATCHED: simulated outputs stopped.")

                    elif event_type == BUTTON_EVENT and number == ENABLE_BUTTON:
                        pressed = bool(value)
                        if pressed and not enable_held:
                            if enabled:
                                enabled = False
                                steering = 0.0
                                throttle = 0.0
                                print("\nController DISABLED.")
                            elif emergency_held:
                                print("\nRelease touchpad before re-enabling.")
                            elif len(seen_axes) != len(axes) or not neutral_for_enable(axes):
                                print("\nRelease stick and triggers before enabling.")
                            else:
                                emergency_latched = False
                                enabled = True
                                print("\nController ENABLED (bench output only).")
                        enable_held = pressed

            now = time.monotonic()
            dt = clamp(now - last_step, 0.0, 0.10)
            last_step = now

            if enabled and not emergency_latched:
                steering = move_toward(
                    steering, steering_percent(axes[STEERING_AXIS]),
                    STEERING_RATE * dt
                )
                requested_throttle = trigger_percent(axes[THROTTLE_AXIS])
                requested_brake = trigger_percent(axes[BRAKE_AXIS])
                # Brake takes priority. Throttle can fall immediately.
                if requested_brake > 5.0:
                    requested_throttle = 0.0
                if requested_throttle < throttle:
                    throttle = requested_throttle
                else:
                    throttle = min(requested_throttle,
                                   throttle + THROTTLE_RISE_RATE * dt)
                brake = requested_brake
            else:
                steering = 0.0
                throttle = 0.0
                # This is only a displayed request, not an actual brake system.
                brake = 100.0 if emergency_latched else 0.0

            if now - last_print >= PRINT_INTERVAL:
                last_print = now
                if emergency_latched:
                    state = "EMERGENCY"
                elif enabled:
                    state = "ENABLED"
                else:
                    state = "DISABLED"
                print(f"{state:9} | steer {steering:+6.1f}% | "
                      f"throttle {throttle:5.1f}% | brake {brake:5.1f}%",
                      flush=True)

    except KeyboardInterrupt:
        print("\nStopped. No vehicle commands were sent.")
    except OSError as error:
        print(f"\nINPUT FAULT: {error}. Stop test; reattach controller before restarting.")
    finally:
        os.close(joystick)


if __name__ == "__main__":
    main()
