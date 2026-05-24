#!/usr/bin/env bash
# Download official MicroPython firmware for ESP32-S3 and prepare for Wokwi
set -euo pipefail

MICROPYTHON_URL="https://micropython.org/resources/firmware/ESP32_GENERIC_S3-20231005-v1.21.0.bin"
FIRMWARE_DIR="build/micropython"
mkdir -p "$FIRMWARE_DIR"

echo "📥 Downloading MicroPython firmware..."
curl -L "$MICROPYTHON_URL" -o "$FIRMWARE_DIR/firmware.bin"

echo "✅ Firmware saved to $FIRMWARE_DIR/firmware.bin"
echo ""
echo "Wokwi and PlatformIO env 'esp32-s3-wokwi' will use this firmware."
echo ""
echo "To flash to real hardware:"
echo "  esptool.py --chip esp32s3 --port <PORT> erase_flash"
echo "  esptool.py --chip esp32s3 --port <PORT> write_flash -z 0x0 $FIRMWARE_DIR/firmware.bin"
echo ""
echo "To deploy Python files:"
  echo "  python3 tools/deploy.py"
