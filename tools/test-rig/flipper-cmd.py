#!/usr/bin/env python3
"""Run a Flipper Zero CLI command and capture output.

Usage:
    python3 tools/test-rig/flipper-cmd.py "help"          # list commands
    python3 tools/test-rig/flipper-cmd.py "top" 2.0       # process snapshot, 2 s wait
    python3 tools/test-rig/flipper-cmd.py "free"          # memory stats

Defaults: WAIT=1.5 s, /dev/ttyACM1, 230400 baud.
"""
import argparse
import re
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cmd", nargs="?", default="?", help="CLI command")
    parser.add_argument(
        "wait", nargs="?", type=float, default=1.5,
        help="seconds to wait after sending (default 1.5)",
    )
    parser.add_argument("--port", default="/dev/ttyACM1")
    parser.add_argument("--baud", type=int, default=230400)
    args = parser.parse_args()

    s = serial.Serial(args.port, args.baud, timeout=0.05)
    time.sleep(0.3)
    s.reset_input_buffer()
    s.write(b"\r\n")
    time.sleep(0.1)
    s.write((args.cmd + "\r\n").encode())
    time.sleep(args.wait)

    buf = []
    while s.in_waiting:
        buf.append(s.read(s.in_waiting).decode("utf-8", errors="replace"))
        time.sleep(0.05)

    s.close()

    out = "".join(buf)
    out = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", out)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
