#!/usr/bin/env python3
"""Capture Flipper Zero CLI log stream for N seconds.

Usage:
    python3 tools/test-rig/flipper-log.py [DURATION_SEC] [--port /dev/ttyACM1]

Defaults: 5.0 seconds, /dev/ttyACM1, 230400 baud (Flipper CLI).

The Flipper exposes two CDC-ACM endpoints; the CLI is the second one.
If the device hops endpoints across reboots, override --port.
"""
import argparse
import re
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "duration", nargs="?", type=float, default=5.0,
        help="seconds to capture (default 5.0)",
    )
    parser.add_argument("--port", default="/dev/ttyACM1")
    parser.add_argument("--baud", type=int, default=230400)
    args = parser.parse_args()

    s = serial.Serial(args.port, args.baud, timeout=0.05)

    # Drain banner.
    time.sleep(0.3)
    s.reset_input_buffer()

    # Enter log streaming.
    s.write(b"\r\n")
    time.sleep(0.1)
    s.write(b"log debug\r\n")
    time.sleep(0.1)

    buf = []
    deadline = time.monotonic() + args.duration
    while time.monotonic() < deadline:
        n = s.in_waiting
        if n:
            buf.append(s.read(n).decode("utf-8", errors="replace"))
        else:
            time.sleep(0.02)

    # Quit log stream cleanly.
    s.write(b"\r\n")
    s.close()

    out = "".join(buf)
    out = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", out)  # strip ANSI escapes
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
