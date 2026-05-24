"""
Base screen class for all calculator screens.
"""


class BaseScreen:
    name = 'base'

    def __init__(self, disp, kpd):
        self.disp = disp
        self.kpd = kpd

    def draw(self):
        raise NotImplementedError

    def handle_key(self, key):
        raise NotImplementedError
