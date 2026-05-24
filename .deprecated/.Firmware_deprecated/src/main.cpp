#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "display/gui.h"
#include "core/keypad.h"
#include "core/settings.h"
#include "screens/menu_system.h"
#include "hal/battery.h"

static QueueHandle_t key_queue;

static void keypad_task(void *pv) {
    (void)pv;
    keypad_init();
    while (1) {
        key_code_t k = keypad_scan();
        if (k != KEY_NONE) {
            xQueueSend(key_queue, &k, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void test_display(void) {
    gui_diag_hardware();
}

static void gui_task(void *pv) {
    (void)pv;
    gui_init();
    battery_init();
    settings_init();

    // Test pattern to verify display works
    Serial.println("[GUI] Drawing test pattern...");

    gui_test_solid(0x00);  // fill entire screen black
    delay(3000);
    gui_test_solid(0xFF);  // fill entire screen white
    delay(3000);

    gui_clear(GUI_COLOR_WHITE);
    gui_rect(10, 10, 276, 108, GUI_COLOR_BLACK, false);  // Border
    gui_text(20, 30, "ESP32 Calc", GUI_FONT_LARGE, GUI_COLOR_BLACK);
    gui_text(20, 60, "Display Test", GUI_FONT_MEDIUM, GUI_COLOR_BLACK);
    gui_text(20, 90, "1234567890", GUI_FONT_SMALL, GUI_COLOR_BLACK);
    gui_end_ex(GUI_REFRESH_FULL);
    Serial.println("[GUI] Test pattern displayed");
    
    delay(3000);  // Show test for 3 seconds

    Serial.println("[GUI] Initializing menu...");
    menu_init();
    Serial.println("[GUI] Drawing menu...");
    bool drew = menu_draw();
    Serial.printf("[GUI] menu_draw returned: %d\n", drew);
    gui_end_ex(GUI_REFRESH_FULL);
    Serial.println("[GUI] Menu displayed");

    key_code_t flush;
    while (xQueueReceive(key_queue, &flush, 0) == pdTRUE);

    bool needs_refresh = false;

    while (1) {
        key_code_t k;
        if (xQueueReceive(key_queue, &k, pdMS_TO_TICKS(50)) == pdTRUE) {
            Serial.printf("[GUI] Key received: %d\n", k);
            do {
                menu_handle_key(k);
            } while (xQueueReceive(key_queue, &k, 0) == pdTRUE);
            needs_refresh = true;
        }

        if (needs_refresh) {
            Serial.println("[GUI] Refreshing display...");
            menu_draw();
            gui_start_refresh();
            needs_refresh = false;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32 Calc starting...");

    key_queue = xQueueCreate(32, sizeof(key_code_t));
    if (!key_queue) {
        Serial.println("Failed to create key queue");
        return;
    }

    xTaskCreatePinnedToCore(keypad_task, "keypad", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(gui_task, "gui", 8192, NULL, 1, NULL, 1);

    Serial.println("Tasks created");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
