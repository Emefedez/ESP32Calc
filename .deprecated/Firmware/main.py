"""
ESP32Calc — MicroPython entry point.
Init HW, run UI loop.
"""
import gc
from time import ticks_ms
from machine import SPI, Pin
from config.pins import (
    PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY,
    PIN_EPD_SCLK, PIN_EPD_MOSI, PIN_EPD_PWR,
    EPD_SPI_BAUD, ROW_PINS, COL_PINS,
)
from drivers.epaper import EPD
from drivers.keypad import Keypad
from drivers.framebuf import Display
from screens.calc_screen import CalcScreen
from screens.menu_screen import MenuScreen
from config.keys import KEY_MODE, KEY_PRIME
from core.scheduler import start as start_scheduler

def ts():
    return ticks_ms()

def log(msg):
    print("{:.3f} ESP32Calc: {}".format(ts() / 1000, msg))

# ── HW ─────────────────────────────────────────────────────────────
log("main start")
start_scheduler()
log("scheduler started on Core 0")
log("epaper pins cs={} dc={} rst={} busy={} sclk={} mosi={} pwr={}".format(
    PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY,
    PIN_EPD_SCLK, PIN_EPD_MOSI, PIN_EPD_PWR))
log("keypad pins rows={} cols={}".format(ROW_PINS, COL_PINS))
Pin(PIN_EPD_PWR, Pin.OUT, value=1)

spi = SPI(2, baudrate=EPD_SPI_BAUD,
          sck=Pin(PIN_EPD_SCLK), mosi=Pin(PIN_EPD_MOSI))

epd = EPD(spi,
          Pin(PIN_EPD_CS, Pin.OUT, value=1),
          Pin(PIN_EPD_DC, Pin.OUT, value=0),
          Pin(PIN_EPD_RST, Pin.OUT, value=0),
          Pin(PIN_EPD_BUSY, Pin.IN))
log("epaper init")
epd.init()
log("epaper ready")

kpd = Keypad(debug=True)
disp = Display(epd)

# ── Screen dispatch ────────────────────────────────────────────────
scr = CalcScreen(disp, kpd)
log("first screen draw start")
scr.draw()
log("first screen drawn")

while True:
    key = kpd.scan()
    if not key:
        gc.collect()
        continue

    log("key {} received on screen {}".format(key, scr.name))

    if key == KEY_PRIME:
        log("pixel probe triggered")
        disp.pixel_probe()
        continue

    if scr.name == 'calc' and key == KEY_MODE:
        log("opening menu")
        scr = MenuScreen(disp, kpd)
        scr.draw()
        log("menu drawn")
        continue

    nxt = scr.handle_key(key)
    log("key {} handled".format(key))
    if nxt is not None:
        scr = nxt
        scr.draw()
        log("next screen drawn")
