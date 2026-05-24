#include "display/gui.h"
#include "hal/pins.h"
#include "hal/battery.h"

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

// ── E-Paper Pins ──
#define EPD_CS      6
#define EPD_DC      7
#define EPD_RST     4
#define EPD_BUSY    5
#define EPD_SCK     16
#define EPD_MOSI    15
#define EPD_PWR     17

#define EPD_WIDTH   128
#define EPD_HEIGHT  296
#define EPD_FB_SIZE (EPD_WIDTH * EPD_HEIGHT / 8)

static bool epd_initialized = false;
static bool partial_lut_loaded = false;

static const uint8_t lut_partial[] = {
    0x00,0x0A,0x00,0x00,0x00,0x01, 0x60,0x14,0x14,0x00,0x00,0x01,
    0x00,0x14,0x00,0x00,0x00,0x01, 0x00,0x13,0x0A,0x01,0x00,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x40,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,
    0x10,0x14,0x0A,0x00,0x00,0x01, 0xA0,0x13,0x01,0x00,0x00,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x40,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,
    0x00,0x14,0x0A,0x00,0x00,0x01, 0x99,0x0C,0x01,0x03,0x04,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x80,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,
    0x20,0x14,0x0A,0x00,0x00,0x01, 0x50,0x13,0x01,0x00,0x00,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x80,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,
    0x20,0x14,0x0A,0x00,0x00,0x01, 0x50,0x13,0x01,0x00,0x00,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,
};

static void epd_cmd(uint8_t c) {
    digitalWrite(EPD_DC, LOW);
    digitalWrite(EPD_CS, LOW);
    SPI.transfer(c);
    digitalWrite(EPD_CS, HIGH);
}

static void epd_data(uint8_t d) {
    digitalWrite(EPD_DC, HIGH);
    digitalWrite(EPD_CS, LOW);
    SPI.transfer(d);
    digitalWrite(EPD_CS, HIGH);
}

static void epd_data_buf(const uint8_t *data, size_t len) {
    digitalWrite(EPD_DC, HIGH);
    digitalWrite(EPD_CS, LOW);
    for (size_t i = 0; i < len; i++) SPI.transfer(data[i]);
    digitalWrite(EPD_CS, HIGH);
}

static void epd_busy(void) {
    delay(10);
    while (digitalRead(EPD_BUSY) == HIGH) delay(1);
}

static void epd_init(void) {
    pinMode(EPD_CS, OUTPUT); digitalWrite(EPD_CS, HIGH);
    pinMode(EPD_DC, OUTPUT); digitalWrite(EPD_DC, LOW);
    pinMode(EPD_RST, OUTPUT); digitalWrite(EPD_RST, HIGH);
    pinMode(EPD_PWR, OUTPUT); digitalWrite(EPD_PWR, HIGH);
    pinMode(EPD_BUSY, INPUT);

    SPI.begin(EPD_SCK, -1, EPD_MOSI, -1);

    digitalWrite(EPD_RST, LOW); delay(20);
    digitalWrite(EPD_RST, HIGH); delay(20);
    epd_busy();

    epd_cmd(0x12); epd_busy();
    epd_cmd(0x01); epd_data(0x27); epd_data(0x01); epd_data(0x00);
    epd_cmd(0x11); epd_data(0x03);
    epd_cmd(0x44); epd_data(0x00); epd_data(0x0F);
    epd_cmd(0x45); epd_data(0x00); epd_data(0x00);
    epd_data(0x27); epd_data(0x01);
    epd_cmd(0x3C); epd_data(0x05);
    epd_cmd(0x21); epd_data(0x00); epd_data(0x80);
    epd_cmd(0x18); epd_data(0x80);

    epd_cmd(0x22); epd_data(0xE0); epd_cmd(0x20); epd_busy();

    epd_cmd(0x24);
    for (int i = 0; i < EPD_FB_SIZE; i++) epd_data(0xFF);
    epd_cmd(0x22); epd_data(0xC7); epd_cmd(0x20); epd_busy();

    epd_initialized = true;
}

static void epd_flush_from_rgb565(const uint16_t *rgb565) {
    static uint8_t fb_1bpp[EPD_FB_SIZE];
    memset(fb_1bpp, 0xFF, sizeof(fb_1bpp));

    for (int ly = 0; ly < 128; ly++) {
        for (int lx = 0; lx < 296; lx++) {
            uint16_t pixel = rgb565[ly * 296 + lx];
            uint8_t r = (pixel >> 11) & 0x1F;
            uint8_t g = (pixel >> 5) & 0x3F;
            uint8_t b = pixel & 0x1F;
            bool black = (r * 77 + g * 150 + b * 29) < 32768;

            int px = ly;
            int py = 295 - lx;
            int idx = (py * 16) + (px / 8);
            int bit = 7 - (px % 8);
            if (black) fb_1bpp[idx] &= ~(1 << bit);
        }
    }

    if (!partial_lut_loaded) {
        epd_cmd(0x32);
        digitalWrite(EPD_DC, HIGH);
        digitalWrite(EPD_CS, LOW);
        for (int i = 0; i < 153; i++) SPI.transfer(lut_partial[i]);
        digitalWrite(EPD_CS, HIGH);
        epd_busy();

        epd_cmd(0x3F); epd_data(lut_partial[153]);
        epd_cmd(0x03); epd_data(lut_partial[154]);
        epd_cmd(0x04);
        epd_data(lut_partial[155]); epd_data(lut_partial[156]);
        epd_data(lut_partial[157]);
        epd_cmd(0x2C); epd_data(lut_partial[158]);
        epd_busy();

        epd_cmd(0x37);
        epd_data(0x00); epd_data(0x00); epd_data(0x00); epd_data(0x00);
        epd_data(0x00); epd_data(0x40); epd_data(0x00); epd_data(0x00);
        epd_data(0x00); epd_data(0x00);
        epd_cmd(0x3C); epd_data(0x80);
        epd_cmd(0x22); epd_data(0xC0); epd_cmd(0x20); epd_busy();
        partial_lut_loaded = true;
    }

    epd_cmd(0x4E); epd_data(0x00);
    epd_cmd(0x4F); epd_data(0x00); epd_data(0x00);
    epd_cmd(0x24);
    epd_data_buf(fb_1bpp, EPD_FB_SIZE);

    epd_cmd(0x22); epd_data(0x0F);
    epd_cmd(0x20);
    epd_busy();
}

void gui_init(void) {
    Serial.println("[GUI] init raylib-canvas SSD1680 backend");

#if defined(EPD_ENABLE_PWR_PIN)
    pinMode(PIN_EPD_PWR, OUTPUT);
    digitalWrite(PIN_EPD_PWR, EPD_PWR_ACTIVE_LEVEL);
    delay(100);
#endif

    epd_init();
    rl_init();

    epd_initialized = true;
    Serial.println("[GUI] init complete");
}

void gui_begin(void) {
    rl_begin_drawing();
    rl_clear_background(WHITE);
}

void gui_clear(gui_color_t color) {
    rl_clear_background(color == GUI_COLOR_BLACK ? BLACK : WHITE);
}

void gui_text(int x, int y, const char *text, gui_font_t font, gui_color_t color) {
    int size = 10 + (int)font * 6;
    rl_draw_text(text, x, y, size, color == GUI_COLOR_BLACK ? BLACK : WHITE);
}

void gui_rect(int x, int y, int w, int h, gui_color_t color, bool fill) {
    Color c = color == GUI_COLOR_BLACK ? BLACK : WHITE;
    if (fill) rl_draw_rectangle(x, y, w, h, c);
    else rl_draw_rectangle_lines(x, y, w, h, c);
}

void gui_line(int x1, int y1, int x2, int y2, gui_color_t color) {
    rl_draw_line(x1, y1, x2, y2, color == GUI_COLOR_BLACK ? BLACK : WHITE);
}

void gui_pixel(int x, int y, gui_color_t color) {
    rl_draw_pixel(x, y, color == GUI_COLOR_BLACK ? BLACK : WHITE);
}

void gui_end(void) { gui_end_ex(GUI_REFRESH_PARTIAL); }

void gui_end_ex(gui_refresh_t mode) {
    if (!epd_initialized) return;
    rl_end_drawing();
    epd_flush_from_rgb565(rl_get_framebuffer());
}

void gui_draw_header(const char *title, bool show_back) {
    rl_draw_rectangle(0, 0, GUI_WIDTH, 20, BLACK);
    rl_draw_text(title, 4, 3, 12, WHITE);
    if (show_back) rl_draw_text("[AC] back", GUI_WIDTH - 90, 3, 10, WHITE);
    battery_reading_t r = battery_read_cached();
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", r.percent);
    rl_draw_text(buf, GUI_WIDTH - 28, 3, 10, BLACK);
}

void gui_test_solid(uint8_t byte) {
    rl_begin_drawing();
    rl_clear_background(byte == 0x00 ? BLACK : WHITE);
    rl_end_drawing();
    epd_flush_from_rgb565(rl_get_framebuffer());
}

void gui_diag_hardware(void) {
    rl_begin_drawing();
    rl_clear_background(WHITE);
    for (int y = 0; y < GUI_HEIGHT; y += 4)
        rl_draw_rectangle(0, y, 64, 2, BLACK);
    rl_draw_text("DIAG MODE", 100, 60, 20, BLACK);
    rl_end_drawing();
    epd_flush_from_rgb565(rl_get_framebuffer());
    while (1) delay(10000);
}

bool gui_is_idle(void) { return true; }
void gui_start_refresh(void) { gui_end_ex(GUI_REFRESH_PARTIAL); }
bool gui_poll_refresh(void) { return true; }
