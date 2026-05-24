#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/pins.h"

#define DEBOUNCE_MS 12
#define HOLD_MS     600
#define HOLD_REPEAT_MS 200

static const uint8_t ROW_PINS[KEY_ROWS] = {
    PIN_KEY_ROW0, PIN_KEY_ROW1, PIN_KEY_ROW2, PIN_KEY_ROW3, PIN_KEY_ROW4,
    PIN_KEY_ROW5, PIN_KEY_ROW6, PIN_KEY_ROW7, PIN_KEY_ROW8,
};
static const uint8_t COL_PINS[KEY_COLS] = {
    PIN_KEY_COL0, PIN_KEY_COL1, PIN_KEY_COL2, PIN_KEY_COL3, PIN_KEY_COL4, PIN_KEY_COL5,
};

typedef enum {
    KEY_NONE = 0,

    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_DOT, KEY_PLUS, KEY_MINUS, KEY_MULT, KEY_DIV,
    KEY_EQUALS, KEY_AC, KEY_DEL,

    KEY_SIN, KEY_COS, KEY_TAN, KEY_LOG, KEY_LN, KEY_SQRT,

    KEY_DIR_UP, KEY_DIR_DOWN, KEY_DIR_LEFT, KEY_DIR_RIGHT,

    KEY_SHIFT, KEY_ALPHA, KEY_MODE, KEY_INTEGRAL, KEY_CALC,
    KEY_DX, KEY_PRIME,
    KEY_XYZ, KEY_XYZ2, KEY_XYZA,
    KEY_FRAC_VERT, KEY_FRAC,
    KEY_LOGAB, KEY_RCL, KEY_ENG,
    KEY_PARENS, KEY_S_D,
    KEY_M_PLUS_MINUS,
    KEY_X10X, KEY_ANS,
    KEY_HYP,

    KEY_COUNT
} key_code_t;

static const key_code_t KEY_MAP[KEY_ROWS][KEY_COLS] = {
    { KEY_SHIFT,     KEY_ALPHA,   KEY_DIR_UP,    KEY_MODE,    KEY_INTEGRAL, KEY_CALC },
    { KEY_DIR_LEFT,  KEY_DIR_DOWN, KEY_DIR_RIGHT, KEY_DX,      KEY_PRIME,    KEY_SQRT },
    { KEY_XYZ,       KEY_XYZ2,    KEY_XYZA,  KEY_FRAC_VERT, KEY_FRAC,   KEY_LOG },
    { KEY_LN,        KEY_LOGAB,   KEY_RCL,   KEY_ENG,     KEY_PARENS,   KEY_S_D },
    { KEY_M_PLUS_MINUS, KEY_7,    KEY_8,     KEY_9,       KEY_DEL,      KEY_AC },
    { KEY_SIN,       KEY_4,       KEY_5,     KEY_6,       KEY_MULT,     KEY_DIV },
    { KEY_COS,       KEY_1,       KEY_2,     KEY_3,       KEY_PLUS,     KEY_MINUS },
    { KEY_TAN,       KEY_0,       KEY_DOT,   KEY_X10X,    KEY_ANS,      KEY_EQUALS },
    { KEY_HYP,       KEY_NONE,    KEY_NONE,  KEY_NONE,    KEY_NONE,     KEY_NONE },
};

void keypad_init(void);
key_code_t keypad_scan(void);
bool keypad_alpha_active(void);
bool keypad_shift_active(void);

#endif
