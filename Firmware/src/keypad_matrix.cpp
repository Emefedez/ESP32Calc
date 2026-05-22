#include "keypad_matrix.h"
#include <Arduino.h>

static unsigned long last_debounce[MATRIX_ROWS][MATRIX_COLS];
static bool last_state[MATRIX_ROWS][MATRIX_COLS];

void keypad_init(void) {
    for (int c = 0; c < MATRIX_COLS; c++) {
        pinMode(COL_PINS[c], INPUT_PULLDOWN);
    }
    for (int r = 0; r < MATRIX_ROWS; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH);
    }
    memset(last_debounce, 0, sizeof(last_debounce));
    memset(last_state, 0, sizeof(last_state));
}

key_code_t keypad_scan(void) {
    unsigned long now = millis();

    for (int r = 0; r < MATRIX_ROWS; r++) {
        digitalWrite(ROW_PINS[r], LOW);
        delayMicroseconds(50);

        for (int c = 0; c < MATRIX_COLS; c++) {
            key_code_t key = KEY_MAP[r][c];
            if (key == KEY_NONE) continue;

            bool pressed = (digitalRead(COL_PINS[c]) == LOW);

            if (pressed != last_state[r][c]) {
                last_debounce[r][c] = now;
                last_state[r][c] = pressed;
            }

            if (pressed && (now - last_debounce[r][c] >= DEBOUNCE_MS)) {
                digitalWrite(ROW_PINS[r], HIGH);
                return key;
            }
        }

        digitalWrite(ROW_PINS[r], HIGH);
    }

    return KEY_NONE;
}
