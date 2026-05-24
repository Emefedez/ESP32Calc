#ifndef RAYLIB_CANVAS_H
#define RAYLIB_CANVAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define RL_CANVAS_WIDTH  296
#define RL_CANVAS_HEIGHT 128

typedef struct {
    uint8_t r, g, b, a;
} Color;

static const Color BLACK   = {0, 0, 0, 255};
static const Color WHITE   = {255, 255, 255, 255};
static const Color RED     = {230, 41, 55, 255};
static const Color GREEN   = {0, 228, 48, 255};
static const Color BLUE    = {0, 121, 241, 255};
static const Color YELLOW  = {250, 200, 0, 255};

// Canvas API (raylib-compatible subset)
void rl_init(void);
void rl_begin_drawing(void);
void rl_end_drawing(void);
void rl_clear_background(Color color);
void rl_draw_pixel(int x, int y, Color color);
void rl_draw_line(int x1, int y1, int x2, int y2, Color color);
void rl_draw_rectangle(int x, int y, int w, int h, Color color);
void rl_draw_rectangle_lines(int x, int y, int w, int h, Color color);
void rl_draw_circle(int x, int y, int radius, Color color);
void rl_draw_text(const char *text, int x, int y, int size, Color color);

// Get raw framebuffer (RGB565)
uint16_t *rl_get_framebuffer(void);
int rl_get_width(void);
int rl_get_height(void);



#ifdef __cplusplus
}
#endif

#endif
