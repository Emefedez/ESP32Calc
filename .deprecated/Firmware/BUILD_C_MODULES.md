# Building Custom MicroPython Firmware with C Modules

The calculator uses Python for UI and keypad, and optionally C modules for
performance-critical math (derivatives, integrals, symbolic math).

## Option 1: Standard MicroPython (no C modules — start here)

1. Flash standard MicroPython for ESP32-S3:
   https://micropython.org/download/ESP32_GENERIC_S3/
2. Upload files: `./deploy.sh`

The calculator works with built-in `math` module for basic operations.
C modules are optional.

## Option 2: Custom Firmware with C Modules

Requires building MicroPython from source.

### Prerequisites

- ESP-IDF v5.x (ESP32-S3 toolchain)
- MicroPython source

### Steps

```bash
# Clone MicroPython
git clone https://github.com/micropython/micropython.git
cd micropython
git submodule update --init

# Build mpy-cross
make -C mpy-cross

# Copy the C module into the ESP32 port
cp -r /path/to/Firmware/modules/calc_math_c ports/esp32/modules/calc_math_c/

# Build ESP32 firmware with C module
cd ports/esp32
make BOARD=ESP32_GENERIC_S3 USER_C_MODULES=modules/calc_math_c/micropython.cmake

# Flash
esptool.py --chip esp32s3 write_flash -z 0x0 build-ESP32_GENERIC_S3/firmware.bin
```

### Using from MicroPython

```python
import calc_math_c
calc_math_c.differentiate("x^2+3*x", "x")
calc_math_c.integrate_numeric(lambda x: x*x, 0, 1)
```
