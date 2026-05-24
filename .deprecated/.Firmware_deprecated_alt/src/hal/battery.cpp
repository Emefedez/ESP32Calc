#include "hal/battery.h"
#include "hal/pins.h"
#include "display/gui.h"
#include <Arduino.h>

#define BATTERY_CACHE_MS 5000
#define BATTERY_LOW_V    3.3f
#define BATTERY_FULL_V   4.2f
#define BATTERY_EMPTY_V  3.0f

static battery_reading_t cached;
static uint32_t last_read = 0;

void battery_init(void) {
    cached.voltage = 0;
    cached.percent = 0;
    last_read = 0;
}

static battery_reading_t battery_read_internal(void) {
    battery_reading_t r;
    int raw = analogRead(PIN_SENSE);
    float adc = (raw / 4095.0f) * 3.3f;
    r.voltage = adc * 2.0f;
    if (r.voltage >= BATTERY_FULL_V) r.percent = 100;
    else if (r.voltage <= BATTERY_EMPTY_V) r.percent = 0;
    else r.percent = (uint8_t)((r.voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f);
    return r;
}

battery_reading_t battery_read(void) {
    return battery_read_internal();
}

battery_reading_t battery_read_cached(void) {
    uint32_t now = millis();
    if (now - last_read >= BATTERY_CACHE_MS) {
        cached = battery_read_internal();
        last_read = now;
    }
    return cached;
}

bool battery_is_low(void) {
    return battery_read_cached().voltage < BATTERY_LOW_V;
}

void battery_force_refresh(void) {
    last_read = 0;
}

void battery_draw_indicator(int x, int y) {
    battery_reading_t r = battery_read_cached();
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", r.percent);
    gui_text(x, y, buf, GUI_FONT_SMALL, GUI_COLOR_BLACK);
}
