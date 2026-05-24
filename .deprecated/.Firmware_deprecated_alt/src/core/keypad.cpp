#include "core/keypad.h"
#include <Arduino.h>
#include <string.h>

static unsigned long last_debounce[KEY_ROWS][KEY_COLS];
static bool last_state[KEY_ROWS][KEY_COLS];
static bool emitted_state[KEY_ROWS][KEY_COLS];

static bool alpha_active = false;
static bool shift_active = false;

bool keypad_alpha_active(void) { return alpha_active; }
bool keypad_shift_active(void) { return shift_active; }

void keypad_init(void) {
    for (int c = 0; c < KEY_COLS; c++) {
        pinMode(COL_PINS[c], INPUT_PULLUP);
    }
    for (int r = 0; r < KEY_ROWS; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH);
    }
    memset(last_debounce, 0, sizeof(last_debounce));
    memset(last_state, 0, sizeof(last_state));
    memset(emitted_state, 0, sizeof(emitted_state));
    alpha_active = false;
    shift_active = false;
}

key_code_t keypad_scan(void) {
    unsigned long now = millis();
    key_code_t returned_key = KEY_NONE;

    for (int r = 0; r < KEY_ROWS; r++) {
        digitalWrite(ROW_PINS[r], LOW);
        delayMicroseconds(50);

        for (int c = 0; c < KEY_COLS; c++) {
            key_code_t key = KEY_MAP[r][c];
            if (key == KEY_NONE) continue;

            bool reading = (digitalRead(COL_PINS[c]) == LOW);

            if (reading != last_state[r][c]) {
                last_debounce[r][c] = now;
            }

            if ((now - last_debounce[r][c]) >= DEBOUNCE_MS) {
                if (reading != emitted_state[r][c]) {
                    emitted_state[r][c] = reading;
                    if (reading) {
                        if (key == KEY_ALPHA) {
                            alpha_active = !alpha_active;
                            continue;
                        }
                        if (key == KEY_SHIFT) {
                            shift_active = !shift_active;
                            continue;
                        }
                        returned_key = key;
                    }
                }
            }

            last_state[r][c] = reading;
        }

        digitalWrite(ROW_PINS[r], HIGH);
    }

    if (returned_key != KEY_NONE && shift_active) {
        shift_active = false;
    }

    return returned_key;
}
