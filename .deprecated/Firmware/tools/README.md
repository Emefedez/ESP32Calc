# Tools

## Real Hardware (ESP32-S3 + MicroPython)

```bash
# Build + flash the frozen firmware
./build.sh

# First flash after old uploaded files: erase once so stale main.py cannot run
./flash.sh --erase /dev/cu.usbmodemXXXX

# Later flashes can skip the erase
./flash.sh /dev/cu.usbmodemXXXX

# Or upload Python files to a stock MicroPython firmware
python3 tools/deploy.py --port /dev/cu.usbmodemXXXX  # via mpremote
# or
pio run -e esp32-s3-upload -t deploypy    # via PlatformIO

# Run the display test with local config/ and drivers/ mounted:
python3 tools/run_display_test.py --port /dev/cu.usbmodemXXXX
```

## Wokwi Simulation

```bash
# Build custom MicroPython firmware with frozen .py modules:
./build.sh

# VS Code / PlatformIO's normal Build task also runs ./build.sh:
platformio run

# Validate diagram.json:
node tools/lint_wokwi.mjs
wokwi-cli lint .

# Then open in Wokwi — wokwi.toml points to build/frozen/firmware.bin.
# Headless `wokwi-cli .` runs require a Wokwi CI API token:
#   set -Ux WOKWI_CLI_TOKEN <token>        # fish
#   export WOKWI_CLI_TOKEN=<token>         # zsh/bash
```

The Docker build uses Espressif's ESP-IDF v5.0.4 image, which matches
MicroPython v1.23.0's ESP32 requirements.

## C Math Extensions (Optional)

See `BUILD_C_MODULES.md` for symbolic differentiation, numeric integration.
