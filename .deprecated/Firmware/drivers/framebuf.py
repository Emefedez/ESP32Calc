"""Framebuffer + EPD bridge."""
from config.display import EPD_WIDTH, EPD_HEIGHT, FB_SIZE
from display.profile import NATIVE_WIDTH, NATIVE_HEIGHT
import framebuf
from time import ticks_diff, ticks_ms

T0 = ticks_ms()
DISPLAY_DEBUG = True


class Display:
    def __init__(self, epd):
        self.epd = epd
        self.buf = bytearray(FB_SIZE)
        self.fb = framebuf.FrameBuffer(self.buf, EPD_WIDTH, EPD_HEIGHT,
                                       framebuf.MONO_VLSB)
        self._shown = False
        self._partial_count = 0

    def clear(self):
        self.fb.fill(0)

    def _color(self, col):
        return 0 if col else 1

    def text(self, s, x, y, col=0):
        self.fb.text(s, x, y, self._color(col))

    def hline(self, x, y, w, col=0):
        self.fb.hline(x, y, w, self._color(col))

    def vline(self, x, y, h, col=0):
        self.fb.vline(x, y, h, self._color(col))

    def rect(self, x, y, w, h, col=0, fill=False):
        if fill:
            self.fb.fill_rect(x, y, w, h, self._color(col))
        else:
            self.fb.rect(x, y, w, h, self._color(col))

    def pixel(self, x, y, col=0):
        self.fb.pixel(x, y, self._color(col))

    def blit(self, buf, x, y):
        self.fb.blit(buf, x, y)

    def refresh(self):
        start = ticks_ms()
        force_full = (not self._shown) or self._partial_count >= 24
        if DISPLAY_DEBUG:
            print("{:.3f} Display: refresh start mode={} shown={} partial_count={}".format(
                ticks_diff(ticks_ms(), T0) / 1000,
                "full" if force_full else "partial",
                self._shown,
                self._partial_count))

        if force_full:
            self.epd.set_full_update()
        else:
            self.epd.set_partial_update()

        self.epd.set_frame_memory(self.buf, 0, 0,
                                  NATIVE_WIDTH, NATIVE_HEIGHT)
        self.epd.display_frame()

        if force_full:
            self._shown = True
            self._partial_count = 0
            if DISPLAY_DEBUG:
                print("{:.3f} Display: refresh done in {}ms next_partial_count={}".format(
                    ticks_diff(ticks_ms(), T0) / 1000,
                    ticks_diff(ticks_ms(), start), self._partial_count))
            return

        self._partial_count += 1
        if DISPLAY_DEBUG:
            print("{:.3f} Display: refresh done in {}ms next_partial_count={}".format(
                ticks_diff(ticks_ms(), T0) / 1000,
                ticks_diff(ticks_ms(), start), self._partial_count))

    def show(self):
        self.refresh()

    def pixel_probe(self):
        """Draw diagnostic pattern of labeled squares at known logical
        positions. Observe where they land on the physical display to
        deduce the correct rotation mapping.
        """
        self.clear()
        # Border around entire 250x128 logical area
        self.rect(0, 0, 249, 127, 0)  # col=0 → black
        self.rect(1, 1, 247, 125, 1)  # col=1 → white (inner border)

        # Center crosshair: 2px wide, 20px long
        cx, cy = 125, 64
        self.hline(cx - 10, cy, 21, 0)
        self.vline(cx, cy - 10, 21, 0)
        self.fb.pixel(cx, cy, 0)

        # 14 numbered squares for position reference
        # Each is a 10x10 filled black square with a 2x2 white hole,
        # placed at known (x,y) logical positions.
        positions = [
            (10,   10,   "A"),   # top-left
            (120,  10,   "B"),   # top-center
            (230,  10,   "C"),   # top-right
            (10,   54,   "D"),   # mid-left
            (230,  54,   "E"),   # mid-right
            (10,   108,  "F"),   # bottom-left
            (120,  108,  "G"),   # bottom-center
            (230,  108,  "H"),   # bottom-right
        ]
        for x, y, label in positions:
            self.fb.fill_rect(x, y, 10, 10, 0)
            self.fb.fill_rect(x + 4, y + 4, 2, 2, 1)
            self.text(label, x, y, 0)

        self.refresh()

        print("Pixel probe drawn. Observe where each square appears.")
        print("Positions: A=tl B=tc C=tr D=ml E=mr F=bl G=bc H=br")
        print("Report which label appears at which physical screen corner.")
