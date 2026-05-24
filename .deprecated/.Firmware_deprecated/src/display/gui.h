#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stdbool.h>

#define GUI_WIDTH  296
#define GUI_HEIGHT 128

typedef enum {
    GUI_COLOR_WHITE = 0,
    GUI_COLOR_BLACK = 1,
} gui_color_t;

typedef enum {
    GUI_FONT_SMALL,
    GUI_FONT_MEDIUM,
    GUI_FONT_LARGE,
    GUI_FONT_XLARGE,
} gui_font_t;

typedef enum {
    GUI_REFRESH_PARTIAL,
    GUI_REFRESH_FULL,
} gui_refresh_t;

void gui_init(void);
void gui_begin(void);
void gui_clear(gui_color_t color);
void gui_text(int x, int y, const char *text, gui_font_t font, gui_color_t color);
void gui_rect(int x, int y, int w, int h, gui_color_t color, bool fill);
void gui_line(int x1, int y1, int x2, int y2, gui_color_t color);
void gui_pixel(int x, int y, gui_color_t color);
void gui_end(void);
void gui_end_ex(gui_refresh_t mode);
void gui_from_rgb565(const uint16_t *pixels);

void gui_draw_header(const char *title, bool show_back);
void gui_diag_hardware(void);
void gui_test_solid(uint8_t byte);

bool gui_is_idle(void);
void gui_start_refresh(void);
bool gui_poll_refresh(void);

#endif
