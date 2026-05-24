#include "display/gui.h"
#include "hal/pins.h"
#include "hal/battery.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>

#if !defined(EPD_SIM_STUB)
#include <SPI.h>
#include <pgmspace.h>
#endif

static GFXcanvas1 canvas(GUI_WIDTH, GUI_HEIGHT);
static bool initialized = false;

#if !defined(EPD_SIM_STUB)
static constexpr uint16_t EPD_RAW_WIDTH = 128;
static constexpr uint16_t EPD_RAW_HEIGHT = 296;
static constexpr uint32_t EPD_FRAME_BYTES =
    uint32_t(EPD_RAW_WIDTH) * uint32_t(EPD_RAW_HEIGHT) / 8;
static constexpr uint16_t FULL_REFRESH_INTERVAL = 32;
static constexpr uint32_t EPD_BUSY_TIMEOUT_MS = 6000;

static SPISettings epd_spi_settings(2000000, MSBFIRST, SPI_MODE0);
static uint8_t epd_frame[EPD_FRAME_BYTES];
static uint16_t partial_count = FULL_REFRESH_INTERVAL;

// IL3820 30-byte LUTs (both Wokwi & real hardware)
static const uint8_t EPD_LUT_FULL[] PROGMEM = {
    0x02,0x02,0x01,0x11,0x12,0x12,0x22,0x22,
    0x66,0x69,0x69,0x59,0x58,0x99,0x99,0x88,
    0x00,0x00,0x00,0x00,0xF8,0xB4,0x13,0x51,
    0x35,0x51,0x51,0x19,0x01,0x00
};
static const uint8_t EPD_LUT_PARTIAL[] PROGMEM = {
    0x00, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x60, 0x14, 0x14, 0x00, 0x00, 0x01, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x13, 0x0A, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x90, 0x14, 0x14, 0x00, 0x00, 0x01, 0x10, 0x14, 0x0A, 0x00, 0x00, 0x01, 0xA0, 0x13, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x90, 0x14, 0x14, 0x00, 0x00, 0x01, 0x00, 0x14, 0x0A, 0x00, 0x00, 0x01, 0x99, 0x0C, 0x01, 0x03, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x90, 0x14, 0x14, 0x00, 0x00, 0x01, 0x20, 0x14, 0x0A, 0x00, 0x00, 0x01, 0x50, 0x13, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define EPD_DBG(fmt, ...) Serial.printf("[EPD] " fmt "\n", ##__VA_ARGS__)

static uint16_t raw_frame_index(uint16_t raw_x, uint16_t raw_y) {
    return raw_y * (EPD_RAW_WIDTH / 8) + raw_x / 8;
}

static void epd_select(void) {
    digitalWrite(PIN_EPD_CS, LOW);
}

static void epd_deselect(void) {
    digitalWrite(PIN_EPD_CS, HIGH);
}

static void epd_command(uint8_t cmd) {
    digitalWrite(PIN_EPD_DC, LOW);
    epd_select();
    SPI.transfer(cmd);
    epd_deselect();
}

static void epd_data(uint8_t data) {
    digitalWrite(PIN_EPD_DC, HIGH);
    epd_select();
    SPI.transfer(data);
    epd_deselect();
}

static void epd_data_buffer(const uint8_t *data, size_t len) {
    digitalWrite(PIN_EPD_DC, HIGH);
    epd_select();
    for (size_t i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }
    epd_deselect();
}

static bool epd_busy(const char *where, uint32_t timeout_ms = EPD_BUSY_TIMEOUT_MS) {
#if defined(EPD_WOKWI_SIM)
    delay(50);
    EPD_DBG("WOKWI: skip busy wait for %s", where);
    return true;
#else
    const uint32_t start = millis();
    int pin_state;
    while (true) {
        pin_state = digitalRead(PIN_EPD_BUSY);
        if (pin_state == LOW) break;
        if (millis() - start > timeout_ms) {
            EPD_DBG("BUSY TIMEOUT in %s after %lu ms (pin=%d)",
                     where, (unsigned long)(millis() - start), pin_state);
            return false;
        }
        delay(5);
    }
    const uint32_t elapsed = millis() - start;
    if (elapsed > 5) {
        EPD_DBG("busy wait %s: %lu ms", where, (unsigned long)elapsed);
    }
    delay(5);
    return true;
#endif
}

static void epd_reset(void) {
    EPD_DBG("reset pulse");
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(20);
    digitalWrite(PIN_EPD_RST, LOW);
    delay(2);
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(20);
    EPD_DBG("reset done");
}

static void epd_set_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    epd_command(0x44);
    epd_data(x0 / 8);
    epd_data(x1 / 8);
    epd_command(0x45);
    epd_data(y0 & 0xFF);
    epd_data(y0 >> 8);
    epd_data(y1 & 0xFF);
    epd_data(y1 >> 8);
}

static void epd_set_pointer(uint16_t x, uint16_t y) {
    epd_command(0x4E);
    epd_data(x / 8);
    epd_command(0x4F);
    epd_data(y & 0xFF);
    epd_data(y >> 8);
}

// IL3820 driver (both Wokwi & real hardware)
static void epd_load_lut(const uint8_t *lut) {
    EPD_DBG("load LUT (30 bytes)");
    epd_command(0x32);
    digitalWrite(PIN_EPD_DC, HIGH);
    epd_select();
    for (size_t i = 0; i < 30; i++) {
        SPI.transfer(pgm_read_byte(&lut[i]));
    }
    epd_deselect();
    epd_busy("lut_write");
    EPD_DBG("LUT loaded");
}
static void epd_init_panel(void) {
    EPD_DBG("=== init start (IL3820) ===");
    epd_reset();
    epd_command(0x01); epd_data(0x27); epd_data(0x01); epd_data(0x00);
    epd_command(0x0C); epd_data(0xD7); epd_data(0xD6); epd_data(0x9D);
    epd_command(0x2C); epd_data(0xA8);
    epd_command(0x3A); epd_data(0x1A);
    epd_command(0x3B); epd_data(0x08);
    epd_command(0x11); epd_data(0x03);
    epd_set_area(0, 0, EPD_RAW_WIDTH - 1, EPD_RAW_HEIGHT - 1);
    epd_set_pointer(0, 0);
    epd_load_lut(EPD_LUT_FULL);
    EPD_DBG("=== init done ===");
}
static bool partial_lut_loaded = false;
static void epd_prep_partial(void) {
    EPD_DBG("partial prep start");
    if (!partial_lut_loaded) {
        // Load 159-byte LUT to register 0x32
        epd_command(0x32);
        digitalWrite(PIN_EPD_DC, HIGH);
        epd_select();
        for (size_t i = 0; i < 153; i++) {
            SPI.transfer(pgm_read_byte(&EPD_LUT_PARTIAL[i]));
        }
        epd_deselect();
        // Control bytes
        epd_command(0x3F); epd_data(pgm_read_byte(&EPD_LUT_PARTIAL[153]));
        epd_command(0x03); epd_data(pgm_read_byte(&EPD_LUT_PARTIAL[154]));
        epd_command(0x04);
        epd_data(pgm_read_byte(&EPD_LUT_PARTIAL[155]));
        epd_data(pgm_read_byte(&EPD_LUT_PARTIAL[156]));
        epd_data(pgm_read_byte(&EPD_LUT_PARTIAL[157]));
        epd_command(0x2C); epd_data(pgm_read_byte(&EPD_LUT_PARTIAL[158]));
        epd_busy("lut_write");
        // OTP selection
        epd_command(0x37);
        epd_data(0x00); epd_data(0x00); epd_data(0x00); epd_data(0x00);
        epd_data(0x00); epd_data(0x40); epd_data(0x00); epd_data(0x00);
        epd_data(0x00); epd_data(0x00);
        // Border follows LUT
        epd_command(0x3C); epd_data(0x80);
        // Power on
        epd_command(0x22); epd_data(0xC0); epd_command(0x20);
        epd_busy("partial_power");
        partial_lut_loaded = true;
    }
    EPD_DBG("partial prep done");
}
static void epd_update_full(void) {
    epd_command(0x22); epd_data(0xC4);
    epd_command(0x20); epd_command(0xFF); epd_busy("full_update");
    EPD_DBG("full update done");
}
static void epd_update_partial(void) {
    epd_command(0x22); epd_data(0x0F);
    epd_command(0x20); epd_busy("partial_update");
    EPD_DBG("partial update done");
}

static bool canvas_pixel_is_black(uint16_t x, uint16_t y) {
    return canvas.getPixel(x, y) != 0;
}

static void build_epd_frame_from_canvas(void) {
    memset(epd_frame, 0xFF, sizeof(epd_frame));

    for (uint16_t y = 0; y < GUI_HEIGHT; y++) {
        for (uint16_t x = 0; x < GUI_WIDTH; x++) {
            if (!canvas_pixel_is_black(x, y)) continue;

            const uint16_t raw_x = y;
            const uint16_t raw_y = x;
            epd_frame[raw_frame_index(raw_x, raw_y)] &= ~(0x80 >> (raw_x & 7));
        }
    }
}

static void epd_write_frame(uint8_t command, const char *label) {
    EPD_DBG("write frame to 0x%02X (%s): area full, ptr=0", command, label);
    epd_set_area(0, 0, EPD_RAW_WIDTH - 1, EPD_RAW_HEIGHT - 1);
    epd_set_pointer(0, 0);
    epd_command(command);
    epd_data_buffer(epd_frame, sizeof(epd_frame));
    EPD_DBG("frame written (%u bytes)", (unsigned)sizeof(epd_frame));
}
#endif

void gui_test_solid(uint8_t byte) {
#if !defined(EPD_SIM_STUB)
    EPD_DBG("solid fill 0x%02X", byte);
    memset(epd_frame, byte, sizeof(epd_frame));
    epd_command(0x11); epd_data(0x03);
    epd_set_area(0, 0, EPD_RAW_WIDTH - 1, EPD_RAW_HEIGHT - 1);
    epd_set_pointer(0, 0);
    epd_command(0x24);
    epd_data_buffer(epd_frame, sizeof(epd_frame));
    epd_command(0x22); epd_data(0xC4);
    epd_command(0x20); epd_command(0xFF);
    epd_busy("solid_fill");
    EPD_DBG("solid fill done");
#else
    (void)byte;
#endif
}

void gui_diag_hardware(void) {
    EPD_DBG("=== DIAG: IL3820 stripe test ===");

    memset(epd_frame, 0xFF, sizeof(epd_frame));
    for (int row = 0; row < 296; row++) {
        for (int col = 0; col < 8; col++) {
            epd_frame[row * 16 + col] = 0x00;
        }
    }

    epd_command(0x11);
    epd_data(0x03);
    epd_set_area(0, 0, EPD_RAW_WIDTH - 1, EPD_RAW_HEIGHT - 1);
    epd_set_pointer(0, 0);
    epd_command(0x24);
    epd_data_buffer(epd_frame, sizeof(epd_frame));

    epd_command(0x22);
    epd_data(0xC4);
    epd_command(0x20);
    epd_command(0xFF);
    epd_busy("diag_update");

    EPD_DBG("stripe displayed");
    while (1) delay(10000);
}

static uint16_t gfx_color(gui_color_t color) {
    return color == GUI_COLOR_BLACK ? 1 : 0;
}

void gui_init(void) {
#if defined(EPD_SIM_STUB)
    Serial.println("[GUI] init simulator stub");
#else
    Serial.println("[GUI] init Waveshare epd2in9_V2 backend");

#if defined(EPD_ENABLE_PWR_PIN)
    Serial.println("[GUI] enabling EPD power pin");
    pinMode(PIN_EPD_PWR, OUTPUT);
    digitalWrite(PIN_EPD_PWR, EPD_PWR_ACTIVE_LEVEL);
    delay(100);
#endif

    pinMode(PIN_EPD_CS, OUTPUT);
    pinMode(PIN_EPD_DC, OUTPUT);
    pinMode(PIN_EPD_RST, OUTPUT);
    pinMode(PIN_EPD_BUSY, INPUT);
    digitalWrite(PIN_EPD_CS, HIGH);

    Serial.printf("[GUI] SPI pins: SCLK=%d MOSI=%d CS=%d\n",
                  PIN_EPD_SCLK, PIN_EPD_MOSI, PIN_EPD_CS);
    Serial.printf("[GUI] EPD pins: RST=%d DC=%d BUSY=%d PWR=%d\n",
                  PIN_EPD_RST, PIN_EPD_DC, PIN_EPD_BUSY, PIN_EPD_PWR);

    SPI.begin(PIN_EPD_SCLK, -1, PIN_EPD_MOSI, -1);
    SPI.beginTransaction(epd_spi_settings);

    Serial.printf("[GUI] BUSY pin reads: %d\n", digitalRead(PIN_EPD_BUSY));

    epd_init_panel();

#endif

    canvas.setTextWrap(false);
    canvas.setTextColor(1);
    canvas.fillScreen(0);
    initialized = true;
    Serial.println("[GUI] init complete");
}

void gui_begin(void) {
    gui_clear(GUI_COLOR_WHITE);
}

void gui_clear(gui_color_t color) {
    canvas.fillScreen(gfx_color(color));
}

void gui_pixel(int x, int y, gui_color_t color) {
    canvas.drawPixel(x, y, gfx_color(color));
}

void gui_line(int x1, int y1, int x2, int y2, gui_color_t color) {
    canvas.drawLine(x1, y1, x2, y2, gfx_color(color));
}

void gui_rect(int x, int y, int w, int h, gui_color_t color, bool fill) {
    if (fill) {
        canvas.fillRect(x, y, w, h, gfx_color(color));
    } else {
        canvas.drawRect(x, y, w, h, gfx_color(color));
    }
}

void gui_text(int x, int y, const char *text, gui_font_t font, gui_color_t color) {
    const uint8_t scale = 1 + static_cast<uint8_t>(font);
    canvas.setTextSize(scale);
    canvas.setTextColor(gfx_color(color));
    canvas.setCursor(x, y);
    canvas.print(text);
}

void gui_from_rgb565(const uint16_t *pixels) {
    for (int y = 0; y < GUI_HEIGHT; y++) {
        for (int x = 0; x < GUI_WIDTH; x++) {
            const uint16_t c = pixels[y * GUI_WIDTH + x];
            const int r5 = (c >> 11) & 0x1F;
            const int g6 = (c >> 5) & 0x3F;
            const int b5 = c & 0x1F;
            const int luma = r5 * 77 + g6 * 150 + b5 * 29;
            gui_pixel(x, y, luma < 32768 ? GUI_COLOR_BLACK : GUI_COLOR_WHITE);
        }
    }
}

void gui_draw_header(const char *title, bool show_back) {
    gui_rect(0, 0, GUI_WIDTH, 20, GUI_COLOR_BLACK, true);
    gui_text(4, 3, title, GUI_FONT_SMALL, GUI_COLOR_WHITE);
    if (show_back) {
        gui_text(GUI_WIDTH - 90, 3, "[AC] back", GUI_FONT_SMALL, GUI_COLOR_WHITE);
    }
    battery_draw_indicator(GUI_WIDTH - 28, 3);
}

void gui_end(void) {
    gui_end_ex(GUI_REFRESH_PARTIAL);
}

void gui_end_ex(gui_refresh_t mode) {
    if (!initialized) return;

#if !defined(EPD_SIM_STUB)
    const bool force_full = mode == GUI_REFRESH_FULL ||
                            partial_count >= FULL_REFRESH_INTERVAL;

    if (force_full) {
        EPD_DBG("full refresh (count=%u)", partial_count);
        build_epd_frame_from_canvas();
        epd_write_frame(0x24, "full");
        epd_update_full();
        partial_count = 0;
    } else {
        EPD_DBG("partial refresh (count=%u)", partial_count);
        epd_prep_partial();
        build_epd_frame_from_canvas();
        epd_write_frame(0x24, "partial");
        epd_update_partial();
        partial_count++;
    }
#else
    (void)mode;
#endif
}

bool gui_is_idle(void) {
    return true;
}

void gui_start_refresh(void) {
    gui_end_ex(GUI_REFRESH_PARTIAL);
}

bool gui_poll_refresh(void) {
    return true;
}
