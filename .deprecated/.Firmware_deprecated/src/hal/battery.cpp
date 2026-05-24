#include "hal/battery.h"
#include <Arduino.h>
#include <esp_adc_cal.h>

#define BATTERY_CACHE_MS   2000
#define BATTERY_CACHE_OPS  10
#define BATTERY_AVG_SAMPLES 4

static esp_adc_cal_characteristics_t adc_chars;
static battery_reading_t cached;
static unsigned long last_read = 0;
static int           access_count = 0;

// Init ADC on GPIO3 (ADC1_CH2) with 11 dB attenuation for 0–3.3V range
void battery_init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_12);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12,
                             ADC_WIDTH_BIT_12, 1100, &adc_chars);
    cached = battery_read();
    last_read = millis();
}

// Read battery voltage (averaged over BATTERY_AVG_SAMPLES ADC reads).
// Default divider ratio = 2 (assumes 100k+100k divider).
// Adjust BATTERY_DIVIDER to match your hardware.
battery_reading_t battery_read(void) {
    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_AVG_SAMPLES; i++) {
        sum += adc1_get_raw(ADC1_CHANNEL_2);
    }
    uint32_t raw = sum / BATTERY_AVG_SAMPLES;
    uint32_t mv  = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    battery_reading_t r;
    // Voltage at battery = ADC_voltage * divider_ratio
    float divider_ratio = 2.0f;
    r.voltage = (mv * divider_ratio) / 1000.0f;

    // Map 3.0 V (empty) … 4.2 V (full) for LiPo
    if      (r.voltage >= 4.2f) r.percent = 100;
    else if (r.voltage <= 3.0f) r.percent = 0;
    else    r.percent = (uint8_t)((r.voltage - 3.0f) / 1.2f * 100.0f);

    return r;
}

battery_reading_t battery_read_cached(void) {
    unsigned long now = millis();
    access_count++;
    if (now - last_read >= BATTERY_CACHE_MS || access_count >= BATTERY_CACHE_OPS) {
        cached = battery_read();
        last_read = now;
        access_count = 0;
    }
    return cached;
}

void battery_force_refresh(void) {
    access_count = BATTERY_CACHE_OPS;  // triggers re-read on next call
}

bool battery_is_low(void) {
    return battery_read_cached().voltage < 3.3f;
}

#include "display/gui.h"

void battery_draw_indicator(int x, int y) {
    battery_reading_t bat = battery_read_cached();
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", bat.percent);
    gui_text(x, y, buf, GUI_FONT_SMALL, GUI_COLOR_WHITE);
}
