"""
SSD1680 e-paper driver for WeAct Studio 2.13" B/W.
Raw SPI, register-level. Based on WeAct Studio reference code.

Architecture:
  - 0x22 register selects subsystem actions; 0x20 fires them; BUSY tracks
  - Full refresh: set counters → write 0x24 → 0x22=0xF7, 0x20
  - Partial refresh: write LUT (once) → set counters → write 0x24 → 0x22=0xCC, 0x20
  - 0x01 data entry mode (X increment, Y decrement) matching WeAct C reference
  - Pre-computed encode map for MONO_VLSB → SSD1680 RAM with 180° rotation
"""
from micropython import const
from time import sleep_ms, ticks_diff, ticks_ms
from array import array

from display.profile import (
    NATIVE_WIDTH, NATIVE_HEIGHT, VISIBLE_WIDTH,
    X_START_BYTE, X_END_BYTE, Y_START, Y_END,
)
from config.display import FB_STRIDE

T0 = ticks_ms()

EPD_WIDTH = NATIVE_WIDTH
EPD_HEIGHT = NATIVE_HEIGHT
EPD_BYTES = EPD_WIDTH * EPD_HEIGHT // 8
_NUM_BYTES_PER_ROW = EPD_WIDTH // 8
EPD_DEBUG = True

# ── Registers ──────────────────────────────────────────────────────
DRIVER_OUTPUT_CONTROL    = const(0x01)
DEEP_SLEEP_MODE          = const(0x10)
DATA_ENTRY_MODE_SETTING  = const(0x11)
SW_RESET                 = const(0x12)
TEMP_SENSOR_CONTROL      = const(0x18)
MASTER_ACTIVATION        = const(0x20)
DISPLAY_UPDATE_CONTROL_1 = const(0x21)
DISPLAY_UPDATE_CONTROL_2 = const(0x22)
WRITE_BW_RAM             = const(0x24)
WRITE_LUT                = const(0x32)
BORDER_WAVEFORM_CONTROL  = const(0x3C)
SET_RAM_X_START_END      = const(0x44)
SET_RAM_Y_START_END      = const(0x45)
SET_RAM_X_COUNTER        = const(0x4E)
SET_RAM_Y_COUNTER        = const(0x4F)

# ── 0x22 values from WeAct reference ──────────────────────────────
C22_PWR_ON   = const(0xF8)  # power on: CP + Clock + LUT + Analog + Display
C22_FULL     = const(0xF7)  # full display refresh
C22_PARTIAL  = const(0xCC)  # partial display refresh (needs custom LUT)
C22_PWR_OFF  = const(0x83)  # power off

BUSY = 1
BUSY_TIMEOUT_MS = 8000



# ── Partial LUT (159 bytes from SSD1680 partial waveform) ─────────
_PARTIAL_LUT = bytes([
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
])


def _ms_since(start):
    return ticks_diff(ticks_ms(), start)


class EPD:
    FULL_UPDATE = const(0)
    PART_UPDATE = const(1)

    def __init__(self, spi, cs, dc, rst, busy, pwr=None):
        self.spi = spi
        self.cs = cs
        self.dc = dc
        self.rst = rst
        self.busy = busy
        self.pwr = pwr
        self.cs.init(self.cs.OUT, value=1)
        self.dc.init(self.dc.OUT, value=0)
        self.rst.init(self.rst.OUT, value=1)
        self.busy.init(self.busy.IN)
        if self.pwr is not None:
            self.pwr.init(self.pwr.OUT, value=1)
        self.width = EPD_WIDTH
        self.visible_width = VISIBLE_WIDTH
        self.height = EPD_HEIGHT
        self.update = None
        self._init_done = False
        self._pending = None
        self._partial_ready = False
        self._lut_loaded = False

    def _debug(self, msg):
        if EPD_DEBUG:
            print("{:.3f} EPD: {}".format(ticks_diff(ticks_ms(), T0) / 1000, msg))

    # ── SPI helpers ────────────────────────────────────────────────
    def _cmd(self, cmd, data=None):
        self.dc(0)
        self.cs(0)
        self.spi.write(bytearray([cmd]))
        self.cs(1)
        if data is not None:
            self._data(data)

    def _data(self, d):
        if isinstance(d, int):
            d = bytearray([d])
        self.dc(1)
        self.cs(0)
        self.spi.write(d)
        self.cs(1)

    # ── Reset / busy wait ──────────────────────────────────────────
    def reset(self):
        self.rst(0)
        sleep_ms(50)
        self.rst(1)
        sleep_ms(50)

    def wait_idle(self, where="wait"):
        start = ticks_ms()
        while self.busy.value() == BUSY:
            if ticks_diff(ticks_ms(), start) > BUSY_TIMEOUT_MS:
                self._debug("timeout {} after {}ms".format(where, _ms_since(start)))
                return False
            sleep_ms(1)
        return True

    wait_until_idle = wait_idle

    # ── Init ───────────────────────────────────────────────────────
    def init(self, update=FULL_UPDATE):
        start = ticks_ms()
        self._debug("init start")
        self.reset()
        self.wait_idle("reset")
        self._cmd(SW_RESET)
        sleep_ms(100)
        self.wait_idle("sw_reset")

        self._cmd(DRIVER_OUTPUT_CONTROL, bytearray([
            (EPD_HEIGHT - 1) & 0xFF,
            ((EPD_HEIGHT - 1) >> 8) & 0xFF,
            0x00,
        ]))
        self._cmd(DATA_ENTRY_MODE_SETTING, b"\x01")
        self._cmd(TEMP_SENSOR_CONTROL, b"\x80")
        self._cmd(BORDER_WAVEFORM_CONTROL, b"\x05")
        self._cmd(DISPLAY_UPDATE_CONTROL_1, b"\x00\x80")
        self._set_full_window()

        self._power_on()

        self._init_done = True
        self.update = None
        self._partial_ready = False
        self._lut_loaded = False
        self._debug("init done in {}ms".format(_ms_since(start)))

    def _power_on(self):
        self._cmd(DISPLAY_UPDATE_CONTROL_2, bytearray([C22_PWR_ON]))
        self._cmd(MASTER_ACTIVATION)
        self.wait_idle("pwr_on")

    def _power_off(self):
        self._cmd(DISPLAY_UPDATE_CONTROL_2, bytearray([C22_PWR_OFF]))
        self._cmd(MASTER_ACTIVATION)
        self.wait_idle("pwr_off")

    # ── Update-mode selectors ──────────────────────────────────────
    def set_full_update(self):
        self.update = self.FULL_UPDATE

    def set_partial_update(self):
        self.update = self.PART_UPDATE

    # ── Frame memory + display ─────────────────────────────────────
    def set_frame_memory(self, image, x=0, y=0, w=None, h=None):
        if not self._init_done:
            self.init(self.FULL_UPDATE)
        self._pending = self._encode(image)

    def display_frame(self):
        if self._pending is None:
            return
        frame = self._pending

        if self.update == self.FULL_UPDATE or not self._partial_ready:
            self._full_update(frame)
            self._partial_ready = True
        else:
            self._partial_update(frame)

        self._pending = None

    def _full_update(self, frame):
        self._debug("full refresh start")
        self._set_full_window()
        self._write_ram(WRITE_BW_RAM, frame)
        self._cmd(DISPLAY_UPDATE_CONTROL_2, bytearray([C22_FULL]))
        self._cmd(MASTER_ACTIVATION)
        self.wait_idle("full_display")
        self._debug("full refresh done")

    def _partial_update(self, frame):
        self._debug("partial refresh start")
        if not self._lut_loaded:
            self._debug("loading partial LUT")
            self._cmd(WRITE_LUT, _PARTIAL_LUT)
            self._cmd(BORDER_WAVEFORM_CONTROL, b"\x80")
            self._lut_loaded = True
        self._set_full_window()
        self._write_ram(WRITE_BW_RAM, frame)
        self._cmd(DISPLAY_UPDATE_CONTROL_2, bytearray([C22_PARTIAL]))
        self._cmd(MASTER_ACTIVATION)
        self.wait_idle("partial_display")
        self._debug("partial refresh done")

    def _activate(self, ctrl, tag=""):
        self._cmd(DISPLAY_UPDATE_CONTROL_2, bytearray([ctrl]))
        self._cmd(MASTER_ACTIVATION)
        self.wait_idle(tag)

    def _write_ram(self, cmd, data):
        self._cmd(cmd)
        self._data(data)

    # ── Encode MONO_VLSB → SSD1680 RAM ─────────────────────────────
    # Data entry mode 0x01 (X inc, Y dec). Counter starts at X=0, Y=249.
    # SPI byte p → RAM (X = p%16, Y = 249 - p//16)
    #
    # framebuf[(r//8)*250 + c] bit (r%8) = pixel at logical (c, r)
    # Logical (c, r) → physical (121-r, 249-c) with 180° rotation
    # RAM(X,Y) bit b → physical (121-(X*8+b), 249-Y)
    # Matching: r = X*8+b, c = Y → RAM(X,Y) = framebuf[X*250 + Y]
    # SPI byte p: source offset = (p%16)*250 + (249 - p//16)
    _ENCODE_MAP = array('H')

    @staticmethod
    def _build_encode_map():
        fb_stride = FB_STRIDE
        m = array('H')
        for p in range(EPD_BYTES):
            x = p & 0x0F
            y = 249 - (p >> 4)
            s = x * fb_stride + y
            m.append(s)
        return m

    def _encode(self, buf):
        start = ticks_ms()
        try:
            im = self.__class__._ENCODE_MAP
        except AttributeError:
            im = None
        if not im:
            self.__class__._ENCODE_MAP = self._build_encode_map()
            im = self.__class__._ENCODE_MAP
        src = memoryview(buf)
        out = bytearray(EPD_BYTES)
        for i in range(EPD_BYTES):
            out[i] = src[im[i]]
        self._debug("encode done in {}ms".format(_ms_since(start)))
        return out

    # ── Misc ───────────────────────────────────────────────────────
    def clear(self, color=0xFF):
        if not self._init_done:
            self.init(self.FULL_UPDATE)
        self._pending = bytearray(bytes([color]) * EPD_BYTES)

    clear_frame_memory = clear

    def sleep(self):
        self._power_off()
        self._cmd(DEEP_SLEEP_MODE, b"\x01")
        self._init_done = False
        self._lut_loaded = False

    # ── Window helpers ─────────────────────────────────────────────
    def _set_full_window(self):
        self._cmd(SET_RAM_X_START_END, bytearray([X_START_BYTE, X_END_BYTE]))
        self._cmd(SET_RAM_Y_START_END, bytearray([
            Y_START & 0xFF, (Y_START >> 8) & 0xFF,
            Y_END & 0xFF, (Y_END >> 8) & 0xFF,
        ]))
        self._cmd(SET_RAM_X_COUNTER, bytearray([X_START_BYTE]))
        self._cmd(SET_RAM_Y_COUNTER, bytearray([
            Y_START & 0xFF, (Y_START >> 8) & 0xFF,
        ]))

    set_memory_area = _set_full_window
    set_memory_pointer = lambda s, x, y: None

    _command = _cmd
    _set_ram_area = _set_full_window
    _set_ram_counter = lambda s, x, y: None
