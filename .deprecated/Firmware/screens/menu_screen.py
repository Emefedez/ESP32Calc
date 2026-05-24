"""
Menu screen (stub).
"""
from screens.base_screen import BaseScreen
from config.keys import KEY_EQUALS, KEY_AC
from config.display import EPD_WIDTH


class MenuScreen(BaseScreen):
    name = 'menu'

    def __init__(self, disp, kpd):
        super().__init__(disp, kpd)
        self.items = ['About', 'Settings', 'Battery', 'Back']
        self.sel = 0

    def draw(self):
        d = self.disp
        d.clear()
        d.text('MENU', 0, 0, 0)
        d.hline(0, 10, EPD_WIDTH, 0)
        for i, item in enumerate(self.items):
            prefix = '> ' if i == self.sel else '  '
            d.text(prefix + item, 0, 20 + i * 18, 0)
        d.show()

    def handle_key(self, key):
        pass
