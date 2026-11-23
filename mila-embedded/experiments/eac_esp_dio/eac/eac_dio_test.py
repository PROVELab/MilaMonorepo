#!/usr/bin/env python3

"""EAC-5000 sink-mode digital-output test.

Designed for libgpiod 1.x and the GPIO mapping identified on this EAC:

    gpiochip0:
        line 146: PAD.00, DIO mode control
        line 147: PAD.01, DIO mode control
        line 81:  PM.05, EXT_OUT0

The program:
    1. Requests and holds the GPIO lines.
    2. Selects the documented NPN/sink mode.
    3. Toggles EXT_OUT0 at a slow test rate.
    4. Prints software timestamps around each GPIO write.

IMPORTANT:
    - Test with the ESP disconnected until the electrical interface
      and power sequencing have been verified.
    - The timestamps bracket the software write call; they do not
      necessarily bracket the physical voltage transition.
    - Linux scheduling means this is not a hardware PWM generator.
"""

import argparse
import ctypes
import ctypes.util
import csv
import signal
import sys
import time


CHIP_NAME = b"gpiochip0"

MODE_PAD00 = 146
MODE_PAD01 = 147
OUTPUT_PM05 = 81

CONSUMER_NAME = b"eac_dio_test"

# On this EAC, the earlier resistor test showed:
#   0 = output sinks
#   1 = output released

OUTPUT_SINK = 0
OUTPUT_RELEASE = 1

stop_requested = False


def request_stop(_signum, _frame):
    global stop_requested
    stop_requested = True


def load_libgpiod():
    library_name = ctypes.util.find_library("gpiod")

    if not library_name:
        raise RuntimeError(
            "Could not find libgpiod. The EAC needs the libgpiod "
            "1.x shared library."
        )

    lib = ctypes.CDLL(library_name, use_errno=True)

    required_functions = (
        "gpiod_chip_open_by_name",
        "gpiod_chip_close",
        "gpiod_chip_get_line",
        "gpiod_line_request_output",
        "gpiod_line_set_value",
        "gpiod_line_release",
    )

    for name in required_functions:
        if not hasattr(lib, name):
            raise RuntimeError(
                f"{name} is missing. This program requires the "
                "libgpiod 1.x API."
            )

    lib.gpiod_chip_open_by_name.argtypes = [ctypes.c_char_p]
    lib.gpiod_chip_open_by_name.restype = ctypes.c_void_p

    lib.gpiod_chip_close.argtypes = [ctypes.c_void_p]
    lib.gpiod_chip_close.restype = None

    lib.gpiod_chip_get_line.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint,
    ]
    lib.gpiod_chip_get_line.restype = ctypes.c_void_p

    lib.gpiod_line_request_output.argtypes = [
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_int,
    ]
    lib.gpiod_line_request_output.restype = ctypes.c_int

    lib.gpiod_line_set_value.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
    ]
    lib.gpiod_line_set_value.restype = ctypes.c_int

    lib.gpiod_line_release.argtypes = [ctypes.c_void_p]
    lib.gpiod_line_release.restype = None

    return lib


def check_result(result, description):
    if result < 0:
        error_number = ctypes.get_errno()
        raise OSError(error_number, f"{description} failed")


def get_line(lib, chip, offset):
    ctypes.set_errno(0)
    line = lib.gpiod_chip_get_line(chip, offset)

    if not line:
        error_number = ctypes.get_errno()
        raise OSError(
            error_number,
            f"Could not get GPIO line {offset}",
        )

    return line


def request_output(lib, line, offset, initial_value):
    ctypes.set_errno(0)

    result = lib.gpiod_line_request_output(
        line,
        CONSUMER_NAME,
        initial_value,
    )

    check_result(result, f"Requesting GPIO line {offset}")


def set_output(lib, line, value):
    ctypes.set_errno(0)

    result = lib.gpiod_line_set_value(line, value)

    check_result(result, "Setting EXT_OUT0")


def wait_seconds(seconds):
    """Wait in small increments so Ctrl+C can stop the test."""
    deadline = time.monotonic() + seconds

    while not stop_requested:
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            return

        time.sleep(min(remaining, 0.05))


def main():
    parser = argparse.ArgumentParser(
        description="Generate slow test pulses on EAC EXT_OUT0."
    )

    parser.add_argument(
        "--frequency",
        type=float,
        default=1.0,
        help="Requested cycles per second; default: 1 Hz",
    )

    parser.add_argument(
        "--duty",
        type=float,
        default=50.0,
        help=(
            "Requested percentage of each cycle spent released/high; "
            "default: 50"
        ),
    )

    args = parser.parse_args()

    if not (0 < args.frequency <= 10):
        parser.error(
            "For this first version, frequency must be greater "
            "than 0 and no more than 10 Hz."
        )

    if not (0 < args.duty < 100):
        parser.error("Duty cycle must be greater than 0 and less than 100.")

    period = 1.0 / args.frequency
    high_seconds = period * args.duty / 100.0
    low_seconds = period - high_seconds

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    lib = load_libgpiod()

    chip = None
    requested_lines = []

    try:
        ctypes.set_errno(0)
        chip = lib.gpiod_chip_open_by_name(CHIP_NAME)

        if not chip:
            error_number = ctypes.get_errno()
            raise OSError(
                error_number,
                "Could not open gpiochip0",
            )

        output = get_line(lib, chip, OUTPUT_PM05)
        pad01 = get_line(lib, chip, MODE_PAD01)
        pad00 = get_line(lib, chip, MODE_PAD00)

        # Request the output first with the value that previously
        # released the external sink during the steady-voltage test.
        request_output(lib, output, OUTPUT_PM05, OUTPUT_RELEASE)
        requested_lines.append(output)

        # Configure the documented NPN/sink-mode values.
        # These requests occur sequentially; startup transitions
        # must still be checked on the real hardware.
        request_output(lib, pad01, MODE_PAD01, 0)
        requested_lines.append(pad01)

        request_output(lib, pad00, MODE_PAD00, 1)
        requested_lines.append(pad00)

        print(
            "Sink mode requested. Starting pulses. "
            "Press Ctrl+C to stop.",
            file=sys.stderr,
        )

        print(
            f"Requested: {args.frequency:g} Hz, "
            f"{args.duty:g}% released/high duty cycle",
            file=sys.stderr,
        )

        writer = csv.writer(sys.stdout)
        writer.writerow([
            "edge_number",
            "commanded_output",
            "software_t_before_ns",
            "software_t_after_ns",
        ])
        sys.stdout.flush()

        edge_number = 0

        while not stop_requested:
            # Command the sink: expected physical falling edge.
            t_before = time.monotonic_ns()
            set_output(lib, output, OUTPUT_SINK)
            t_after = time.monotonic_ns()

            writer.writerow([
                edge_number,
                "sink",
                t_before,
                t_after,
            ])
            sys.stdout.flush()
            edge_number += 1

            wait_seconds(low_seconds)

            if stop_requested:
                break

            # Release the sink: expected physical rising edge.
            t_before = time.monotonic_ns()
            set_output(lib, output, OUTPUT_RELEASE)
            t_after = time.monotonic_ns()

            writer.writerow([
                edge_number,
                "release",
                t_before,
                t_after,
            ])
            sys.stdout.flush()
            edge_number += 1

            wait_seconds(high_seconds)

    finally:
        if requested_lines:
            # Best effort: request release while the output line
            # is still owned. Its state after process exit is not
            # guaranteed.
            try:
                set_output(lib, requested_lines[0], OUTPUT_RELEASE)
            except Exception as exc:
                print(
                    f"Could not request output release: {exc}",
                    file=sys.stderr,
                )

        for line in reversed(requested_lines):
            lib.gpiod_line_release(line)

        if chip:
            lib.gpiod_chip_close(chip)

        print(
            "GPIO lines released. Their state after exit is not guaranteed.",
            file=sys.stderr,
        )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)