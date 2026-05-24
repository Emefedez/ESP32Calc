"""
Main calculator input/result screen.
"""
from screens.base_screen import BaseScreen
from core.calc_math import CalcMath, KEY_TO_CHAR
from config.keys import KEY_EQUALS, KEY_AC, KEY_DEL
from config.display import EPD_WIDTH, EPD_HEIGHT


class CalcScreen(BaseScreen):
    name = 'calc'

    def __init__(self, disp, kpd):
        super().__init__(disp, kpd)
        self.math = CalcMath()

    def draw(self):
        self.math.poll_result()
        d = self.disp
        d.clear()

        # Status line
        status = []
        if self.kpd.shift:
            status.append('S')
        if self.kpd.alpha:
            status.append('A')
        if status:
            d.text(' '.join(status), 0, 0, 0)
        d.hline(0, 10, EPD_WIDTH, 0)

        # Expression — scroll visible portion
        vis = int(EPD_WIDTH / 8)
        off = max(0, len(self.math.expr) - vis)
        d.text(self.math.expr[off:], 0, 20, 0)

        # Result or error
        y = 40
        if self.math.error:
            d.text('Err:', 0, y, 0)
            d.text(self.math.error[:16], 30, y, 0)
        elif self.math.result is not None:
            d.text('=', 0, y, 0)
            d.text(str(self.math.result)[:16], 15, y, 0)
            y += 18
            more = str(self.math.result)[16:]
            if more:
                d.text(more[:16], 0, y, 0)

        d.show()

    def handle_key(self, key):
        self.math.handle_key(key)
        self.draw()
