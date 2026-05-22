#include "gui.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMono24pt7b.h>

#define EPD_CS   6
#define EPD_DC   7
#define EPD_RST  4
#define EPD_BUSY 5
#define EPD_SCK  16
#define EPD_MOSI 15
#define EPD_PWR  17

static GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> display(
    GxEPD2_290_T94_V2(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

static const GFXfont *font_map[] = {
    [GUI_FONT_SMALL]  = (GFXfont *)&FreeMono9pt7b,
    [GUI_FONT_MEDIUM] = (GFXfont *)&FreeMono12pt7b,
    [GUI_FONT_LARGE]  = (GFXfont *)&FreeMono18pt7b,
    [GUI_FONT_XLARGE] = (GFXfont *)&FreeMono24pt7b,
};

static int cursor_y_offset[] = {
    [GUI_FONT_SMALL]  = 7,
    [GUI_FONT_MEDIUM] = 10,
    [GUI_FONT_LARGE]  = 14,
    [GUI_FONT_XLARGE] = 19,
};

void gui_init(void) {
    pinMode(EPD_PWR, OUTPUT);
    digitalWrite(EPD_PWR, HIGH);
    delay(100);

    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

    display.init(115200, true, 10, false);
    display.setRotation(1);
    display.setFullWindow();
}

void gui_begin(void) {
    display.setFullWindow();
}

void gui_clear(gui_color_t color) {
    display.fillScreen(color == GUI_COLOR_BLACK ? GxEPD_BLACK : GxEPD_WHITE);
}

static uint16_t to_epd_color(gui_color_t c) {
    return (c == GUI_COLOR_BLACK) ? GxEPD_BLACK : GxEPD_WHITE;
}

void gui_text(int x, int y, const char *text, gui_font_t font, gui_color_t color) {
    display.setFont(font_map[font]);
    display.setTextColor(to_epd_color(color));
    display.setCursor(x, y + cursor_y_offset[font]);
    display.print(text);
}

void gui_rect(int x, int y, int w, int h, gui_color_t color, bool fill) {
    uint16_t c = to_epd_color(color);
    if (fill)
        display.fillRect(x, y, w, h, c);
    else
        display.drawRect(x, y, w, h, c);
}

void gui_line(int x1, int y1, int x2, int y2, gui_color_t color) {
    display.drawLine(x1, y1, x2, y2, to_epd_color(color));
}

void gui_pixel(int x, int y, gui_color_t color) {
    display.drawPixel(x, y, to_epd_color(color));
}

void gui_end(void) {
    display.display(false);
}
