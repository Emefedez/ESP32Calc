#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <soc/timer_group_reg.h>

#include "keypad_matrix.h"
#include "calc_math.h"
#include "gui.h"

static QueueHandle_t key_queue;
static TaskHandle_t key_task_handle;
static TaskHandle_t gui_task_handle;

static void keypad_task(void *pv) {
    (void)pv;
    keypad_init();

    while (1) {
        key_code_t k = keypad_scan();
        if (k != KEY_NONE) {
            xQueueSend(key_queue, &k, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void gui_task(void *pv) {
    (void)pv;
    gui_init();

    key_code_t buf[16];
    int count = 0;

    while (1) {
        key_code_t k;
        if (xQueueReceive(key_queue, &k, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (count < 16) buf[count++] = k;

            gui_begin();
            gui_clear(GUI_COLOR_WHITE);

            gui_rect(0, 0, GUI_WIDTH, 20, GUI_COLOR_BLACK, true);
            gui_text(4, 3, "ESP32-S3 Calc", GUI_FONT_SMALL, GUI_COLOR_WHITE);

            char tmp[64] = {0};
            for (int i = 0; i < count; i++) {
                char c[8];
                snprintf(c, sizeof(c), "%d ", buf[i]);
                strncat(tmp, c, sizeof(tmp) - strlen(tmp) - 1);
            }
            gui_text(4, 30, tmp, GUI_FONT_SMALL, GUI_COLOR_BLACK);

            gui_end();
        }
    }
}

static void disable_iwdt() {
    REG_WRITE(TIMG_WDTWPROTECT_REG(0), 0x50D83AA1);
    REG_WRITE(TIMG_WDTCONFIG0_REG(0), 0);
    REG_WRITE(TIMG_WDTWPROTECT_REG(0), 0);
}

void setup() {
    disable_iwdt();
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32 Calc starting...");

    key_queue = xQueueCreate(16, sizeof(key_code_t));
    if (!key_queue) {
        Serial.println("Failed to create queue");
        return;
    }

    xTaskCreatePinnedToCore(keypad_task, "keypad", 4096, NULL, 2, &key_task_handle, 1);
    xTaskCreatePinnedToCore(gui_task, "gui", 8192, NULL, 1, &gui_task_handle, 1);

    Serial.println("Tasks created");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
