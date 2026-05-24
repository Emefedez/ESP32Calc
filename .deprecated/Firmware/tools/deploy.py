#!/usr/bin/env python3
"""
Upload MicroPython files to ESP32-S3 via mpremote.
Run standalone:  python3 tools/deploy.py [--port /dev/cu.usbmodemXXXX]
Or via PlatformIO: pio run -e esp32-s3-upload
"""
import argparse
import glob
import os
import subprocess
import sys

# Detect project root (works standalone and via PlatformIO SCons)
if "__file__" in dir():
    PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
elif "PROJECT_DIR" in os.environ:
    PROJ = os.environ["PROJECT_DIR"]
else:
    PROJ = os.getcwd()

FILES = [
    "boot.py", "main.py",
    "config/__init__.py", "config/pins.py", "config/keys.py",
    "config/layout.py", "config/display.py",
    "drivers/__init__.py", "drivers/epaper.py",
    "drivers/keypad.py", "drivers/framebuf.py",
    "core/__init__.py", "core/calc_math.py", "core/settings.py",
    "screens/__init__.py", "screens/base_screen.py",
    "screens/calc_screen.py", "screens/menu_screen.py",
    "screens/about_screen.py", "screens/battery_screen.py",
]

DIRS = ["config", "drivers", "core", "screens"]


def likely_ports():
    patterns = (
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/cu.SLAB_USBtoUART*",
        "/dev/cu.wchusbserial*",
    )
    ports = []
    for pattern in patterns:
        ports.extend(glob.glob(pattern))
    return sorted(set(ports))


def port_args(port):
    return ["connect", port] if port else []


def mp(port, *args):
    r = subprocess.run([sys.executable, "-m", "mpremote"] + port_args(port) + list(args),
                       capture_output=True, text=True)
    if r.returncode:
        err = r.stderr.strip() or r.stdout.strip()
        print(f"  \u26a0 {err}")
    return r.returncode


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Deploy MicroPython files with mpremote.")
    parser.add_argument("--port", default=os.environ.get("MPREMOTE_PORT"),
                        help="serial port, e.g. /dev/cu.usbmodemXXXX")
    args = parser.parse_args()

    port = args.port
    ports = likely_ports()
    if not port and len(ports) == 1:
        port = ports[0]
    elif not port and len(ports) > 1:
        print("Multiple likely ESP32 serial ports found:")
        for candidate in ports:
            print(f"  {candidate}")
        print("Run again with --port /dev/cu.usbmodemXXXX")
        sys.exit(2)
    elif not port:
        print("No likely ESP32 serial port found.")
        print("Connect the board, put it in boot/USB mode if needed, then retry.")
        print("Override with --port /dev/cu.usbmodemXXXX or MPREMOTE_PORT=...")
        sys.exit(2)

    print("Deploying to ESP32-S3...")
    print(f"  port: {port}")
    for d in DIRS:
        mp(port, "mkdir", f":{d}")
    ok = True
    for src in FILES:
        path = os.path.join(PROJ, src)
        if os.path.exists(path):
            if mp(port, "cp", path, f":{src}"):
                print(f"  \u274c {src}")
                ok = False
        else:
            print(f"  \u26a0 missing {src}")
    if ok:
        print("Done. Reset device to run.")
    else:
        sys.exit(1)
