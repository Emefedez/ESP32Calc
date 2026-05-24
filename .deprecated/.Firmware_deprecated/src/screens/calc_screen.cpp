#include "calc_screen.h"
#include "display/gui.h"
#include "core/settings.h"
#include "core/keypad.h"
#include "menu_system.h"
#include <stdio.h>
#include <string.h>

static key_code_t calc_buf[32];
static int        calc_len = 0;

static void enter(screen_t *self) { (void)self; calc_len = 0; }

static const char *lbl(key_code_t k) {
    switch (k) {
    case KEY_SHIFT:     return "SHI";
    case KEY_ALPHA:     return "ALP";
    case KEY_UP:        return "UP";
    case KEY_MODE:      return "MOD";
    case KEY_INTEGRAL:  return "INT";
    case KEY_CALC:      return "CAL";
    case KEY_LEFT:      return "LT";
    case KEY_DOWN:      return "DN";
    case KEY_RIGHT:     return "RT";
    case KEY_DX:        return "dx";
    case KEY_PRIME:     return "'''";
    case KEY_SQRT:      return "V";
    case KEY_XYZ:       return "xyz";
    case KEY_XYZ2:      return "x2";
    case KEY_XYZA:      return "x^a";
    case KEY_FRAC_VERT: return "FV";
    case KEY_FRAC:      return "F/";
    case KEY_LOG:       return "log";
    case KEY_LN:        return "ln";
    case KEY_LOGAB:     return "logab";
    case KEY_RCL:       return "RCL";
    case KEY_ENG:       return "ENG";
    case KEY_PARENS:    return "()";
    case KEY_S_D:       return "SD";
    case KEY_M_PLUS_MINUS: return "M+-";
    case KEY_0:         return "0";
    case KEY_1:         return "1";
    case KEY_2:         return "2";
    case KEY_3:         return "3";
    case KEY_4:         return "4";
    case KEY_5:         return "5";
    case KEY_6:         return "6";
    case KEY_7:         return "7";
    case KEY_8:         return "8";
    case KEY_9:         return "9";
    case KEY_DOT:       return ".";
    case KEY_PLUS:      return "+";
    case KEY_MINUS:     return "-";
    case KEY_MULT:      return "*";
    case KEY_DIV:       return "/";
    case KEY_EQUALS:    return "=";
    case KEY_AC:        return "AC";
    case KEY_DEL:       return "DEL";
    case KEY_SIN:       return "sin";
    case KEY_COS:       return "cos";
    case KEY_TAN:       return "tan";
    case KEY_X10X:      return "10^x";
    case KEY_ANS:       return "Ans";
    case KEY_HYP:       return "hyp";
    default:            return "???";
    }
}

static void draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    gui_draw_header(self->title, true);

    char tmp[128] = {0};
    int pos = 0;
    for (int i = 0; i < calc_len; i++) {
        const char *s = lbl(calc_buf[i]);
        int slen = strlen(s);
        if (pos + slen + 2 < (int)sizeof(tmp)) {
            if (pos > 0) tmp[pos++] = ' ';
            memcpy(tmp + pos, s, slen);
            pos += slen;
        }
    }
    gui_text(4, 28, tmp, GUI_FONT_SMALL, GUI_COLOR_BLACK);

    // Angle mode indicator (far right)
    char ang[8];
    snprintf(ang, sizeof(ang), "%s",
             g_settings.angle_mode == ANGLE_DEG ? "DEG" : "RAD");
    gui_text(GUI_WIDTH - 30, 28, ang, GUI_FONT_SMALL, GUI_COLOR_BLACK);

    // Alpha / Shift indicator
    if (keypad_alpha_active()) {
        gui_text(4, 70, "ALPHA", GUI_FONT_SMALL, GUI_COLOR_BLACK);
    } else if (keypad_shift_active()) {
        gui_text(4, 70, "SHIFT", GUI_FONT_SMALL, GUI_COLOR_BLACK);
    }
}

static void key(screen_t *self, key_code_t k) {
    (void)self;
    if (k == KEY_MODE) {
        menu_pop_to_root();
    } else if (k == KEY_AC) {
        if (calc_len > 0) calc_len = 0;
        else              menu_pop();
    } else if (k == KEY_DEL && calc_len > 0) {
        calc_len--;
    } else {
        if (calc_len < 32) calc_buf[calc_len++] = k;
    }
}

static screen_t screen = {
    .title = "Calculator",
    .on_enter = enter, .on_draw = draw, .on_key = key,
};

screen_t *screen_calc_create(void) { return &screen; }
