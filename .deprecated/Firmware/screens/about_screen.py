"""
About screen (stub).
"""
from screens.base_screen import BaseScreen
from config.keys import KEY_AC


class AboutScreen(BaseScreen):
    name = 'about'

    def draw(self):
        d = self.disp
        d.clear()
        d.text('ESP32Calc', 10, 40, 0)
        d.text('MicroPython', 10, 60, 0)
        d.text('AC to exit', 10, 100, 0)
        d.show()

    def handle_key(self, key):
        if key == KEY_AC:
            from screens.calc_screen import CalcScreen
            return CalcScreen(self.disp, self.kpd)
        return self
