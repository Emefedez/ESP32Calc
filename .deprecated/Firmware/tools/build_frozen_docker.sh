#!/usr/bin/env bash
# Build MicroPython firmware with frozen modules using Docker.
# Output: build/frozen/firmware.bin (for Wokwi + real HW)
#
# Requires: Docker Desktop installed and running.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJ_DIR/build/frozen"
MODULES_DIR="$BUILD_DIR/modules"
DOCKER_IMG="esp32calc-mpy-builder:idf5.0.4-mpy1.23"
DOCKER_BUILD_DIR="$BUILD_DIR/docker"
FIRMWARE_BIN="$BUILD_DIR/firmware.bin"
FIRMWARE_ELF="$BUILD_DIR/firmware.elf"
LOCK_DIR="$BUILD_DIR/.build.lock"

echo "=== ESP32Calc Frozen Firmware Builder (Docker) ==="
echo ""

mkdir -p "$MODULES_DIR" "$DOCKER_BUILD_DIR"

if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    echo "Another frozen firmware build appears to be running."
    echo "If it is stale, stop the old builder container and remove: $LOCK_DIR"
    exit 1
fi
trap 'rm -rf "$LOCK_DIR"' EXIT

# ── Step 1: Copy Python files ─────────────────────────────────────────
echo "[1/3] Preparing Python modules..."
rm -rf "$MODULES_DIR"/*
rm -rf "$BUILD_DIR/output"

for f in \
    boot.py main.py \
    config/__init__.py config/pins.py config/keys.py \
    config/layout.py config/display.py \
    display/__init__.py display/profile.py \
    drivers/__init__.py drivers/epaper.py \
    drivers/keypad.py drivers/framebuf.py \
    core/__init__.py core/calc_math.py core/settings.py core/scheduler.py \
    screens/__init__.py screens/base_screen.py \
    screens/calc_screen.py screens/menu_screen.py \
    screens/about_screen.py screens/battery_screen.py; do
    mkdir -p "$MODULES_DIR/$(dirname "$f")"
    cp "$PROJ_DIR/$f" "$MODULES_DIR/$f"
done

# ── Step 2: Build Docker image ────────────────────────────────────────
echo "[2/3] Building Docker image (first time only)..."
docker build --progress=plain -t "$DOCKER_IMG" -f "$SCRIPT_DIR/Dockerfile.build" "$SCRIPT_DIR"

# ── Step 3: Build firmware inside Docker ──────────────────────────────
echo "[3/3] Building firmware in Docker..."

cat > "$DOCKER_BUILD_DIR/build_cmds.sh" << 'ENDSCRIPT'
#!/bin/bash
set -euo pipefail

: "${IDF_PATH:=/opt/esp/idf}"
: "${IDF_TOOLS_PATH:=/opt/esp/tools}"
export IDF_PATH IDF_TOOLS_PATH

# Clone MicroPython
if [ ! -d /build/micropython/.git ]; then
    git clone --depth 1 --branch v1.23.0 \
        https://github.com/micropython/micropython.git /build/micropython
    git -C /build/micropython submodule update --init --depth 1 \
        lib/berkeley-db-1.xx lib/micropython-lib
fi

# Build mpy-cross in its own directory. The ESP32 port also uses a BUILD
# variable, so pass this explicitly to keep generated ESP32 headers out of
# the host-side mpy-cross build.
rm -rf /build/micropython/mpy-cross/build
make -C /build/micropython/mpy-cross \
     BUILD=/build/micropython/mpy-cross/build \
     -j$(nproc)
export MICROPY_MPYCROSS=/build/micropython/mpy-cross/build/mpy-cross

# Generate manifest
cat > /build/manifest.py << 'MANIFEST'
include("$(PORT_DIR)/boards/manifest.py")
freeze("/build/modules")
MANIFEST

# Build ESP32 firmware
cd /build/micropython/ports/esp32
source "$IDF_PATH/export.sh" >/dev/null
make BOARD=ESP32_GENERIC_S3 BUILD=/build/esp32_build submodules
rm -rf /build/esp32_build
make BOARD=ESP32_GENERIC_S3 \
     FROZEN_MANIFEST=/build/manifest.py \
     BUILD=/build/esp32_build \
     -j$(nproc)

# Copy result
mkdir -p /build/output
cp /build/esp32_build/firmware.bin /build/output/firmware.bin
cp /build/esp32_build/micropython.elf /build/output/firmware.elf
echo "BUILD_OK" > /build/output/status
ENDSCRIPT

chmod +x "$DOCKER_BUILD_DIR/build_cmds.sh"

docker run --rm -v "$BUILD_DIR:/build" "$DOCKER_IMG" \
    bash /build/docker/build_cmds.sh 2>&1

# ── Result ────────────────────────────────────────────────────────────
if [ -f "$BUILD_DIR/output/firmware.bin" ]; then
    cp "$BUILD_DIR/output/firmware.bin" "$FIRMWARE_BIN"
    cp "$BUILD_DIR/output/firmware.elf" "$FIRMWARE_ELF"
    echo ""
    echo "=== Done ==="
    echo "Firmware: $FIRMWARE_BIN ($(ls -lh "$FIRMWARE_BIN" | awk '{print $5}'))"
    echo "ELF:      $FIRMWARE_ELF ($(ls -lh "$FIRMWARE_ELF" | awk '{print $5}'))"
    echo "For Wokwi: wokwi.toml -> build/frozen/firmware.bin"
    echo "For real HW: esptool.py --chip esp32s3 write_flash -z 0x0 $FIRMWARE_BIN"
else
    echo "Build failed. Check Docker logs above."
    exit 1
fi
