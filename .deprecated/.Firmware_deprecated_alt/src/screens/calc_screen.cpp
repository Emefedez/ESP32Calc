#include "screens/calc_screen.h"
#include "display/gui.h"
#include "core/calc_math.h"
#include "core/settings.h"
#include <Arduino.h>
#include <string.h>
#include <cstdio>

#define MAX_EXPR 64
#define MAX_RESULT 32

static char expression[MAX_EXPR];
static char result[MAX_RESULT];
static int expr_len = 0;

void calc_screen_enter(screen_t *self) {
    expression[0] = '\0';
    result[0] = '\0';
    expr_len = 0;
    self->dirty = true;
}

static const char *key_label(key_code_t k) {
    switch (k) {
        case KEY_0: return "0"; case KEY_1: return "1"; case KEY_2: return "2";
        case KEY_3: return "3"; case KEY_4: return "4"; case KEY_5: return "5";
        case KEY_6: return "6"; case KEY_7: return "7"; case KEY_8: return "8";
        case KEY_9: return "9";
        case KEY_DOT: return "."; case KEY_PLUS: return "+"; case KEY_MINUS: return "-";
        case KEY_MULT: return "*"; case KEY_DIV: return "/"; case KEY_EQUALS: return "=";
        case KEY_AC: return "AC"; case KEY_DEL: return "DEL";
        case KEY_SIN: return "sin"; case KEY_COS: return "cos"; case KEY_TAN: return "tan";
        case KEY_LOG: return "log"; case KEY_LN: return "ln"; case KEY_SQRT: return "sqrt";
        case KEY_PARENS: return "()"; case KEY_X10X: return "E"; case KEY_ANS: return "Ans";
        case KEY_DIR_UP: return "UP"; case KEY_DIR_DOWN: return "DN";
        case KEY_DIR_LEFT: return "LT"; case KEY_DIR_RIGHT: return "RT";
        case KEY_HYP: return "hyp"; case KEY_MODE: return "MODE";
        default: return "?";
    }
}

static void append_key(key_code_t k) {
    const char *lbl = key_label(k);
    int len = strlen(lbl);
    if (expr_len + len < MAX_EXPR - 1) {
        strcpy(expression + expr_len, lbl);
        expr_len += len;
        expression[expr_len] = '\0';
    }
}

void calc_screen_draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    gui_rect(0, 20, GUI_WIDTH, 1, GUI_COLOR_BLACK, true);
    gui_text(10, 35, expression, GUI_FONT_MEDIUM, GUI_COLOR_BLACK);
    if (result[0]) gui_text(10, 70, result, GUI_FONT_LARGE, GUI_COLOR_BLACK);
    const char *mode = (g_settings.angle_mode == ANGLE_DEG) ? "DEG" : "RAD";
    gui_text(GUI_WIDTH - 50, 3, mode, GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

void calc_screen_handle_key(screen_t *self, key_code_t key) {
    self->dirty = true;
    if (key == KEY_AC) { expression[0] = '\0'; result[0] = '\0'; expr_len = 0; return; }
    if (key == KEY_DEL) {
        if (expr_len > 0) {
            expression[expr_len] = '\0'; expr_len--;
            while (expr_len > 0 && expression[expr_len-1] >= '0' && expression[expr_len-1] <= '9') expr_len--;
            expression[expr_len] = '\0';
        }
        return;
    }
    if (key == KEY_EQUALS) { snprintf(result, sizeof(result), "TODO"); return; }
    if (key == KEY_MODE) { settings_toggle_angle(); return; }
    append_key(key);
}
