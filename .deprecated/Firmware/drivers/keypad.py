"""
Matrix keypad driver for ESP32Calc.
9 rows × 6 cols, columns PULL_UP, rows driven LOW to scan.
"""
from machine import Pin
from time import ticks_ms, sleep_us
from config.pins import ROW_PINS, COL_PINS, KEY_ROWS, KEY_COLS
from config.layout import KEY_MAP
from config.keys import KEY_NONE, KEY_SHIFT, KEY_ALPHA

DEBOUNCE_MS = 12
HOLD_MS     = 600
HOLD_REPEAT_MS = 200


class Keypad:
    def __init__(self, debug=False):
        self._cols = [Pin(p, Pin.IN, Pin.PULL_UP) for p in COL_PINS]
        self._rows = [Pin(p, Pin.OUT, value=1) for p in ROW_PINS]
        self._last_db = [[0] * KEY_COLS for _ in range(KEY_ROWS)]
        self._last_st = [[False] * KEY_COLS for _ in range(KEY_ROWS)]
        self._emit_st = [[False] * KEY_COLS for _ in range(KEY_ROWS)]
        self._alpha = False
        self._shift = False
        self._debug = debug
        if self._debug:
            print("Keypad: rows={} cols={}".format(ROW_PINS, COL_PINS))
            print("Keypad: initial col values={}".format(
                tuple(col.value() for col in self._cols)))

    @property
    def alpha(self):
        return self._alpha

    @property
    def shift(self):
        return self._shift

    def scan(self):
        now = ticks_ms()
        result = KEY_NONE

        for r in range(KEY_ROWS):
            self._rows[r].value(0)
            sleep_us(50)

            for c in range(KEY_COLS):
                k = KEY_MAP[r][c]
                if k == KEY_NONE:
                    continue

                reading = self._cols[c].value() == 0

                if reading != self._last_st[r][c]:
                    self._last_db[r][c] = now

                if now - self._last_db[r][c] >= DEBOUNCE_MS:
                    if reading != self._emit_st[r][c]:
                        self._emit_st[r][c] = reading
                        if reading:
                            if self._debug:
                                print("Keypad: press row={} col={} key={}".format(
                                    r, c, k))
                            if k == KEY_ALPHA:
                                self._alpha = not self._alpha
                                if self._debug:
                                    print("Keypad: alpha={}".format(self._alpha))
                                continue
                            if k == KEY_SHIFT:
                                self._shift = not self._shift
                                if self._debug:
                                    print("Keypad: shift={}".format(self._shift))
                                continue
                            result = k

                self._last_st[r][c] = reading

            self._rows[r].value(1)

        if result != KEY_NONE and self._shift:
            self._shift = False
            if self._debug:
                print("Keypad: shift auto-clear")

        return result
