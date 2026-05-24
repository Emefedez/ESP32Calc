"""
Dual-core task allocation.

Core 0 (PRO_CPU, _thread): Calc engine + future WiFi/BLE.
Core 1 (APP_CPU, main):   GUI (keypad, display) + battery sense.

Mailboxes use locks for thread-safe IPC.  The calc worker
sleeps when idle so Core 1 isn't starved.
"""
import _thread
from time import sleep_ms


class _MBox:
    __slots__ = ('lock', '_val', '_full')

    def __init__(self):
        self.lock = _thread.allocate_lock()
        self._val = None
        self._full = False

    def put(self, v):
        self.lock.acquire()
        self._val = v
        self._full = True
        self.lock.release()

    def get(self, default=None):
        self.lock.acquire()
        v = self._val if self._full else default
        self._val = None
        self._full = False
        self.lock.release()
        return v


_expr_mb = _MBox()   # Main → Calc: expression string to evaluate
_res_mb  = _MBox()   # Calc → Main: (result, error, ans)


def _calc_worker():
    from core.calc_math import CalcMath
    cm = CalcMath()
    while True:
        expr = _expr_mb.get(None)
        if expr is None:
            sleep_ms(1)
            continue
        cm.expr = expr
        cm._eval()
        _res_mb.put((cm.result, cm.error, cm.ans))


def start():
    _thread.start_new_thread(_calc_worker, ())


def eval_expr(expr):
    _expr_mb.put(expr)


def poll_result(default=None):
    return _res_mb.get(default)
