"""
Calculator math engine — safe expression evaluation.

Expression building runs on Core 1 (main thread).
Evaluation runs on Core 0 (_thread) via core.scheduler.
"""
import math
from core.scheduler import eval_expr, poll_result
from config.keys import (
    KEY_PLUS, KEY_MINUS, KEY_MULT, KEY_DIV,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_DOT, KEY_EQUALS, KEY_AC, KEY_DEL,
    KEY_SIN, KEY_COS, KEY_TAN, KEY_LOG, KEY_LN, KEY_SQRT,
    KEY_PARENS,
)

ALLOWED = {
    'sin':   math.sin,
    'cos':   math.cos,
    'tan':   math.tan,
    'asin':  math.asin,
    'acos':  math.acos,
    'atan':  math.atan,
    'sqrt':  math.sqrt,
    'log':   math.log,
    'log10': math.log10,
    'exp':   math.exp,
    'abs':   abs,
    'pi':    math.pi,
    'e':     math.e,
}

KEY_TO_CHAR = {
    KEY_0: '0', KEY_1: '1', KEY_2: '2', KEY_3: '3', KEY_4: '4',
    KEY_5: '5', KEY_6: '6', KEY_7: '7', KEY_8: '8', KEY_9: '9',
    KEY_DOT: '.', KEY_PLUS: '+', KEY_MINUS: '-',
    KEY_MULT: '*', KEY_DIV: '/',
    KEY_SIN: 'sin(', KEY_COS: 'cos(', KEY_TAN: 'tan(',
    KEY_LOG: 'log10(', KEY_LN: 'log(', KEY_SQRT: 'sqrt(',
}


class CalcMath:
    def __init__(self):
        self.expr = ''
        self.result = None
        self.ans = None
        self.error = None
        self._eval_pending = False

    def key_to_char(self, key):
        return KEY_TO_CHAR.get(key)

    def handle_key(self, key):
        if key == KEY_AC:
            self.expr = ''
            self.result = None
            self.error = None
            self._eval_pending = False
            return
        if key == KEY_DEL:
            self.expr = self.expr[:-1]
            self.result = None
            self.error = None
            self._eval_pending = False
            return
        if key == KEY_EQUALS:
            eval_expr(self.expr)
            self._eval_pending = True
            return
        ch = KEY_TO_CHAR.get(key)
        if ch:
            self.expr += ch
            self.result = None
            self.error = None
            self._eval_pending = False

    def poll_result(self):
        if not self._eval_pending:
            return
        res = poll_result()
        if res is not None:
            r, err, ans = res
            self.result = r
            self.error = err
            self.ans = ans
            self._eval_pending = False

    def _eval(self):
        try:
            code = compile(self.expr, '<calc>', 'eval')
            for name in code.co_names:
                if name not in ALLOWED:
                    raise NameError(f"'{name}' not allowed")
            self.result = eval(code, {"__builtins__": {}}, ALLOWED)
            self.ans = self.result
            self.error = None
        except Exception as e:
            self.result = None
            self.error = str(e)
