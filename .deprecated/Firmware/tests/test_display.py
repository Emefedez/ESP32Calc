"""
Display test.

Preferred:
    python3 tools/run_display_test.py --port /dev/cu.usbmodemXXXX

Plain `mpremote run tests/test_display.py` only works if config/ and drivers/
are already frozen into firmware or uploaded to the board filesystem.
"""
import sys

for path in ("/remote", "", "."):
    if path not in sys.path:
        sys.path.insert(0, path)

import framebuf  # type: ignore[import-untyped]
from machine import SPI, Pin  # type: ignore[import-untyped]
from time import sleep_ms  # type: ignore[attr-defined]

try:
    from config.pins import *
    from config.display import EPD_WIDTH, EPD_HEIGHT, FB_SIZE
    from config.pins import EPD_SPI_BAUD
    from display.profile import NATIVE_WIDTH as EPD_NATIVE_WIDTH
    from display.profile import NATIVE_HEIGHT as EPD_NATIVE_HEIGHT
    from drivers.epaper import EPD
except ImportError as exc:
    print("Import failed:", exc)
    print("Run from Firmware with:")
    print("  python3 tools/run_display_test.py --port /dev/cu.usbmodemXXXX")
    print("or deploy/freeze config/ and drivers/ before using mpremote run.")
    raise

Pin(PIN_EPD_PWR, Pin.OUT, value=1)

spi = SPI(2, baudrate=EPD_SPI_BAUD, sck=Pin(PIN_EPD_SCLK),
          mosi=Pin(PIN_EPD_MOSI))

epd = EPD(spi, Pin(PIN_EPD_CS, Pin.OUT, value=1),
          Pin(PIN_EPD_DC, Pin.OUT, value=0),
          Pin(PIN_EPD_RST, Pin.OUT, value=0),
          Pin(PIN_EPD_BUSY, Pin.IN))
epd.init()

W, H = EPD_WIDTH, EPD_HEIGHT
buf = bytearray(FB_SIZE)
fb = framebuf.FrameBuffer(buf, W, H, framebuf.MONO_VLSB)

# Phase 1: all black. The project framebuffer uses bit 1 as logical black;
# the driver rotates that into the panel's native 128x250 RAM format.
fb.fill(1)
epd.set_full_update()
epd.set_frame_memory(buf, 0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT)
epd.display_frame()
print("All black drawn")
sleep_ms(2000)

# Phase 2: all white
fb.fill(0)
epd.set_full_update()
epd.set_frame_memory(buf, 0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT)
epd.display_frame()
print("All white drawn")
sleep_ms(2000)

# Phase 3: text
fb.fill(0)
fb.text("ESP32Calc", 16, 40, 1)
fb.text("MicroPython", 16, 60, 1)
fb.text("Hello!", 16, 80, 1)
epd.set_partial_update()
epd.set_frame_memory(buf, 0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT)
epd.display_frame()
print("Text drawn")
sleep_ms(2000)

print("Test complete")
