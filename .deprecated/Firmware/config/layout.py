# Physical keypad layout — mirrors Firmware_deprecated/src/core/keypad.h KEY_MAP
# Access by KEY_MAP[row][col]

from config.keys import (
    KEY_NONE,
    KEY_SHIFT, KEY_ALPHA, KEY_UP, KEY_MODE, KEY_INTEGRAL, KEY_CALC,
    KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_DX, KEY_PRIME, KEY_SQRT,
    KEY_XYZ, KEY_XYZ2, KEY_XYZA, KEY_FRAC_VERT, KEY_FRAC, KEY_LOG,
    KEY_LN, KEY_LOGAB, KEY_RCL, KEY_ENG, KEY_PARENS, KEY_S_D,
    KEY_M_PLUS_MINUS, KEY_7, KEY_8, KEY_9, KEY_DEL, KEY_AC,
    KEY_SIN, KEY_4, KEY_5, KEY_6, KEY_MULT, KEY_DIV,
    KEY_COS, KEY_1, KEY_2, KEY_3, KEY_PLUS, KEY_MINUS,
    KEY_TAN, KEY_0, KEY_DOT, KEY_X10X, KEY_ANS, KEY_EQUALS,
    KEY_HYP,
)

KEY_MAP = (
    (KEY_SHIFT,     KEY_ALPHA,   KEY_UP,    KEY_MODE,    KEY_INTEGRAL, KEY_CALC),
    (KEY_LEFT,      KEY_DOWN,    KEY_RIGHT, KEY_DX,      KEY_PRIME,    KEY_SQRT),
    (KEY_XYZ,       KEY_XYZ2,    KEY_XYZA,  KEY_FRAC_VERT, KEY_FRAC,   KEY_LOG),
    (KEY_LN,        KEY_LOGAB,   KEY_RCL,   KEY_ENG,     KEY_PARENS,   KEY_S_D),
    (KEY_M_PLUS_MINUS, KEY_7,    KEY_8,     KEY_9,       KEY_DEL,      KEY_AC),
    (KEY_SIN,       KEY_4,       KEY_5,     KEY_6,       KEY_MULT,     KEY_DIV),
    (KEY_COS,       KEY_1,       KEY_2,     KEY_3,       KEY_PLUS,     KEY_MINUS),
    (KEY_TAN,       KEY_0,       KEY_DOT,   KEY_X10X,    KEY_ANS,      KEY_EQUALS),
    (KEY_HYP,       KEY_NONE,    KEY_NONE,  KEY_NONE,    KEY_NONE,     KEY_NONE),
)
