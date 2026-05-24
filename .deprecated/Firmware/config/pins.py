try:
    from micropython import const
except ImportError:
    const = lambda value: value

# ── E-Paper Display (WeAct Studio 2.13" B/W / SSD1680) ─────────────
PIN_EPD_CS   = const(6)
PIN_EPD_DC   = const(7)
PIN_EPD_RST  = const(4)
PIN_EPD_BUSY = const(5)
PIN_EPD_SCLK = const(16)
PIN_EPD_MOSI = const(15)
PIN_EPD_PWR  = const(17)

EPD_SPI_BAUD = const(8_000_000)

# ── Keypad Matrix (9 rows × 6 cols) ──────────────────────────────
PIN_KEY_ROW0 = const(38)
PIN_KEY_ROW1 = const(37)
PIN_KEY_ROW2 = const(36)
PIN_KEY_ROW3 = const(35)
PIN_KEY_ROW4 = const(0)
PIN_KEY_ROW5 = const(9)
PIN_KEY_ROW6 = const(10)
PIN_KEY_ROW7 = const(11)
PIN_KEY_ROW8 = const(12)

ROW_PINS = (PIN_KEY_ROW0, PIN_KEY_ROW1, PIN_KEY_ROW2, PIN_KEY_ROW3,
            PIN_KEY_ROW4, PIN_KEY_ROW5, PIN_KEY_ROW6, PIN_KEY_ROW7,
            PIN_KEY_ROW8)
KEY_ROWS = const(9)

PIN_KEY_COL0 = const(45)
PIN_KEY_COL1 = const(8)
PIN_KEY_COL2 = const(47)
PIN_KEY_COL3 = const(21)
PIN_KEY_COL4 = const(14)
PIN_KEY_COL5 = const(13)

COL_PINS = (PIN_KEY_COL0, PIN_KEY_COL1, PIN_KEY_COL2, PIN_KEY_COL3,
            PIN_KEY_COL4, PIN_KEY_COL5)
KEY_COLS = const(6)

# ── SD Card (SPI) ─────────────────────────────────────────────────
PIN_SD_CS   = const(42)
PIN_SD_MOSI = const(41)
PIN_SD_SCLK = const(40)
PIN_SD_MISO = const(39)

# ── Sense / Battery ADC ──────────────────────────────────────────
PIN_SENSE = const(3)

# ── Serial ────────────────────────────────────────────────────────
PIN_SERIAL_TX = const(43)
PIN_SERIAL_RX = const(44)
