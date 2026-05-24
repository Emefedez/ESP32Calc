#!/usr/bin/env bash
# Flash the combined frozen MicroPython firmware image to an ESP32-S3.
set -euo pipefail

cd "$(dirname "$0")"

FIRMWARE="build/frozen/firmware.bin"
ERASE=0

if [ "${1:-}" = "--erase" ]; then
    ERASE=1
    shift
fi

PORT="${1:-${PORT:-}}"

if [ ! -f "$FIRMWARE" ]; then
    echo "Missing $FIRMWARE; building it first."
    ./build.sh
fi

if [ -z "$PORT" ]; then
    ports=(/dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART* /dev/cu.usbserial*)
    for candidate in "${ports[@]}"; do
        if [ -e "$candidate" ]; then
            PORT="$candidate"
            break
        fi
    done
fi

if [ -z "$PORT" ]; then
    echo "No serial port found. Usage: ./flash.sh [--erase] /dev/cu.usbmodemXXXX"
    exit 1
fi

if [ "$ERASE" = "1" ]; then
    python3 -m esptool --chip esp32s3 --port "$PORT" erase_flash
fi

python3 -m esptool --chip esp32s3 --port "$PORT" write_flash -z 0x0 "$FIRMWARE"
