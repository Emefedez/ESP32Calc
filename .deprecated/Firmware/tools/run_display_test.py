#!/usr/bin/env python3
"""Run tests/test_display.py with the Firmware directory mounted on device."""
import argparse
import glob
import os
import subprocess
import sys

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def likely_ports():
    ports = []
    for pattern in (
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/cu.SLAB_USBtoUART*",
        "/dev/cu.wchusbserial*",
    ):
        ports.extend(glob.glob(pattern))
    return sorted(set(ports))


def resolve_port(port):
    if port:
        return port
    ports = likely_ports()
    if len(ports) == 1:
        return ports[0]
    if len(ports) > 1:
        print("Multiple likely ESP32 serial ports found:")
        for candidate in ports:
            print(f"  {candidate}")
        print("Run again with --port /dev/cu.usbmodemXXXX")
        sys.exit(2)
    print("No likely ESP32 serial port found.")
    print("Connect the board, put it in boot/USB mode if needed, then retry.")
    print("Override with --port /dev/cu.usbmodemXXXX or MPREMOTE_PORT=...")
    sys.exit(2)


def main():
    parser = argparse.ArgumentParser(description="Run display test via mpremote mount.")
    parser.add_argument("--port", default=os.environ.get("MPREMOTE_PORT"),
                        help="serial port, e.g. /dev/cu.usbmodemXXXX")
    args = parser.parse_args()
    port = resolve_port(args.port)

    code = (
        "import os, sys\n"
        "sys.path.insert(0, '/remote')\n"
        "os.chdir('/remote')\n"
        "exec(open('/remote/tests/test_display.py').read())\n"
    )
    cmd = [
        sys.executable, "-m", "mpremote",
        "connect", port,
        "mount", PROJ,
        "exec", code,
    ]
    print(f"Running display test on {port} with {PROJ} mounted.")
    raise SystemExit(subprocess.call(cmd, cwd=PROJ))


if __name__ == "__main__":
    main()
