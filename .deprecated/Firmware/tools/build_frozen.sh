#!/usr/bin/env bash
# Build MicroPython firmware with frozen ESP32Calc modules.
# Output: build/frozen/firmware.bin (for Wokwi + real HW)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJ_DIR/build/frozen"
MODULES_DIR="$BUILD_DIR/modules"
MP_SRC="$BUILD_DIR/micropython"
ESP32_BUILD="$BUILD_DIR/esp32_build"
FIRMWARE_BIN="$BUILD_DIR/firmware.bin"
MANIFEST="$BUILD_DIR/manifest.py"

echo "=== ESP32Calc Frozen Firmware Builder ==="

# ── PlatformIO toolchains ──────────────────────────────────────────────
PIO_DIR="$HOME/.platformio"
ESPIDF_DIR="$PIO_DIR/packages/framework-espidf"
export PATH="$PIO_DIR/packages/toolchain-xtensa-esp32s3/bin:$PIO_DIR/packages/tool-cmake/bin:$PIO_DIR/packages/tool-ninja/bin:$PATH"
export IDF_PATH="$ESPIDF_DIR"
export IDF_TOOLS_PATH="$BUILD_DIR/idf_tools"

mkdir -p "$MODULES_DIR" "$BUILD_DIR"

# ── Step 1: Clone MicroPython ─────────────────────────────────────────
if [ ! -d "$MP_SRC/.git" ]; then
    echo "[1/4] Cloning MicroPython v1.23.0..."
    git clone --depth 1 --branch v1.23.0 \
        https://github.com/micropython/micropython.git "$MP_SRC"
    git -C "$MP_SRC" submodule update --init --depth 1 \
        lib/berkeley-db-1.xx lib/micropython-lib
else
    echo "[1/4] MicroPython already present"
fi

# ── Step 2: Build mpy-cross ───────────────────────────────────────────
if [ ! -f "$MP_SRC/mpy-cross/build/mpy-cross" ]; then
    echo "[2/4] Building mpy-cross..."
    make -C "$MP_SRC/mpy-cross" -j"$(nproc)"
else
    echo "[2/4] mpy-cross ready"
fi

# ── Step 3: Copy + .mpy-compile all Python files ──────────────────────
echo "[3/4] Preparing frozen modules..."
rm -rf "$MODULES_DIR"/*

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

cd "$MODULES_DIR"
for f in $(find . -name '*.py' -not -name 'boot.py'); do
    "$MP_SRC/mpy-cross/build/mpy-cross" "$f"
done
cd "$PROJ_DIR"

# Generate manifest
cat > "$MANIFEST" << EOF
freeze("$MODULES_DIR")
EOF

# ── Step 4: Build ESP32 firmware via IDF ──────────────────────────────
echo "[4/4] Building firmware..."
cd "$MP_SRC/ports/esp32"

# Source IDF env
set +u
source "$IDF_PATH/export.sh" 2>/dev/null || true
set -u

# Fix Apple Clang -Werror issue on macOS
CMAKE_EXTRA=""
[ "$(uname)" = "Darwin" ] && CMAKE_EXTRA="-DEXTRA_CFLAGS=-Wno-error=gnu-folding-constant"

make clean 2>/dev/null || true
make BOARD=ESP32_GENERIC_S3 \
     FROZEN_MANIFEST="$MANIFEST" \
     BUILD="$ESP32_BUILD" \
     CMAKE_ARGS="$CMAKE_EXTRA" \
     -j"$(nproc)"

cp "$ESP32_BUILD/firmware.bin" "$FIRMWARE_BIN" 2>/dev/null || true

echo "=== Done ==="
[ -f "$FIRMWARE_BIN" ] && echo "Firmware: $FIRMWARE_BIN ($(ls -lh "$FIRMWARE_BIN" | awk '{print $5}'))" \
                      || echo "⚠️  Build failed – check $ESP32_BUILD"
