"""
PlatformIO extra script for MicroPython upload.
Called from platformio.ini: extra_scripts = pre:tools/deploy_pio.py

Usage: pio run -e esp32-s3-upload -t deploypy

This script delegates to tools/deploy.py so PlatformIO and standalone
uploads share the same mpremote/device detection behavior.
"""
import os
import subprocess
import sys

try:
    Import("env")  # noqa: F821 - SCons magic
except NameError:
    print("deploy_pio.py is a PlatformIO extra script, not a standalone command.")
    print("Use: python3 tools/deploy.py --port /dev/cu.usbmodemXXXX")
    print("Or:  pio run -e esp32-s3-upload -t deploypy")
    sys.exit(2)

PROJ = env.subst("$PROJECT_DIR")  # noqa: F821


def deploy_action(target, source, env):
    python = os.environ.get("PYTHON", "python3")
    cmd = [python, os.path.join(PROJ, "tools", "deploy.py")]
    upload_port = env.GetProjectOption("upload_port", "")
    if upload_port:
        cmd.extend(["--port", upload_port])
    return subprocess.call(cmd, cwd=PROJ)


env.AlwaysBuild(env.Alias("deploypy", [], deploy_action))  # noqa: F821
