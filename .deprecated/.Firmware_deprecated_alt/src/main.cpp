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

static void gui_task(void *pv) {
    (void)pv;
    gui_init();
    battery_init();
    settings_init();

    // Display test
    gui_test_solid(0x00);
    delay(1500);
    gui_test_solid(0xFF);
    delay(500);

    menu_init();
    menu_draw();
    gui_end_ex(GUI_REFRESH_FULL);

    key_code_t flush;
    while (xQueueReceive(key_queue, &flush, 0) == pdTRUE);

    bool needs_refresh = false;

    while (1) {
        key_code_t k;
        if (xQueueReceive(key_queue, &k, pdMS_TO_TICKS(50)) == pdTRUE) {
            Serial.printf("[GUI] Key: %d\n", k);
            do {
                menu_handle_key(k);
            } while (xQueueReceive(key_queue, &k, 0) == pdTRUE);
            needs_refresh = true;
        }

        if (needs_refresh) {
            if (menu_draw()) {
                gui_start_refresh();
            }
            needs_refresh = false;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32 Calc starting (raylib)...");

    key_queue = xQueueCreate(32, sizeof(key_code_t));
    if (!key_queue) {
        Serial.println("Failed to create key queue");
        return;
    }

    xTaskCreatePinnedToCore(keypad_task, "keypad", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(gui_task, "gui", 16384, NULL, 1, NULL, 1);

    Serial.println("Tasks created");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
