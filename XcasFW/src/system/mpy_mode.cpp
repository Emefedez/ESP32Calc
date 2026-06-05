#include "system/mpy_mode.h"

#include <cstdio>
#include <cstring>

#include "app_config.h"
#include "app_events.h"
#include "display/weact_213_bw.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "graphics/mono_canvas.h"
#include "hardware/keymap.h"
#include "system/boot_mode.h"
#include "system/micropython_runtime.h"
#include "system/storage_manager.h"

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "mpy_mode";
constexpr size_t kTextBufSize = 4096;
constexpr int kStatusBarY = 0;
constexpr int kStatusBarH = 13;
constexpr int kTextStartY = kStatusBarY + kStatusBarH + 2;
constexpr int kCharW = 6;
constexpr int kCharH = 8;
constexpr int kMaxLines = (MonoCanvas::kHeight - kTextStartY) / kCharH;
constexpr size_t kSourceBytes = 8192;

char g_text_buf[kTextBufSize] {};
size_t g_text_len = 0;

void stdout_capture(const char* str, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    const unsigned char ch = static_cast<unsigned char>(str[i]);
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (g_text_len == 0 || g_text_buf[g_text_len - 1] != '\n') {
        if (g_text_len < kTextBufSize - 1) {
          g_text_buf[g_text_len++] = '\n';
        }
      }
      continue;
    }
    if (ch < 32 || ch > 126) continue;
    if (g_text_len < kTextBufSize - 1) {
      g_text_buf[g_text_len++] = static_cast<char>(ch);
    }
  }
  if (g_text_len >= kTextBufSize - 1) {
    g_text_buf[kTextBufSize - 1] = '\0';
  } else {
    g_text_buf[g_text_len] = '\0';
  }
}

size_t count_lines_from_end(const char* buf, size_t len, int max_lines) {
  size_t lines_found = 0;
  size_t pos = len;
  while (pos > 0 && lines_found <= static_cast<size_t>(max_lines)) {
    --pos;
    if (buf[pos] == '\n') {
      ++lines_found;
    }
  }
  return pos;
}

void render_terminal(Weact213BwDisplay& display) {
  MonoCanvas canvas;
  canvas.begin_frame(true);
  canvas.clear(true);

  // canvas.draw_text(2, kStatusBarY, "MPY", 1, true);
  // canvas.hline(0, kStatusBarH, MonoCanvas::kWidth, true);

  int line_start = static_cast<int>(count_lines_from_end(g_text_buf, g_text_len, kMaxLines));
  int display_y = kTextStartY;
  while (display_y < MonoCanvas::kHeight - kCharH && line_start < static_cast<int>(g_text_len)) {
    int line_end = line_start;
    while (line_end < static_cast<int>(g_text_len) && g_text_buf[line_end] != '\n') {
      ++line_end;
    }
    int len = line_end - line_start;
    if (len > 0 && line_start + len <= static_cast<int>(g_text_len)) {
      char saved = g_text_buf[line_start + len];
      g_text_buf[line_start + len] = '\0';
      canvas.draw_text(4, display_y, &g_text_buf[line_start], 1, true);
      g_text_buf[line_start + len] = saved;
    }
    display_y += kCharH;
    line_start = line_end + 1;
  }

  canvas.draw_text(205, MonoCanvas::kHeight - kCharH - 1, "AC=EXIT", 1, true);
  display.update_canvas(canvas);
}

bool read_source_file(const char* path, char* output, size_t output_size) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  output[0] = '\0';
  FILE* file = std::fopen(path, "rb");
  if (file == nullptr) {
    return false;
  }
  const size_t used = std::fread(output, 1, output_size - 1, file);
  const int extra = std::fgetc(file);
  std::fclose(file);
  if (extra != EOF) {
    output[0] = '\0';
    return false;
  }
  output[used] = '\0';
  return true;
}

void show_error_and_reboot(Weact213BwDisplay& display, const char* msg) {
  MonoCanvas canvas;
  canvas.begin_frame(true);
  canvas.clear(true);
  canvas.draw_text(2, 2, "MPY ERROR", 1, true);
  canvas.draw_text(4, 20, msg, 1, true);
  canvas.draw_text(4, 50, "REBOOTING...", 1, true);
  display.update_canvas(canvas);
  vTaskDelay(pdMS_TO_TICKS(2000));
  boot::set_mode(boot::Mode::Calculator);
  esp_restart();
}

void run_app(StorageManager& storage,
             Weact213BwDisplay& display,
             QueueHandle_t app_events,
             const ExternalAppManifest& manifest) {
  clear_key_overrides();
  storage.apply_global_keymap();
  storage.apply_app_keymap(manifest.id);

  char entry_path[160] {};
  std::snprintf(entry_path, sizeof(entry_path), "%s/%s", manifest.source_path, manifest.entry);

  char source[kSourceBytes] {};
  if (!read_source_file(entry_path, source, sizeof(source))) {
    show_error_and_reboot(display, "read failed");
    return;
  }

  size_t heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  void* heap = heap_caps_malloc(heap_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (heap == nullptr) {
    heap_size = 256 * 1024;
    heap = heap_caps_malloc(heap_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  if (heap == nullptr) {
    show_error_and_reboot(display, "heap fail");
    return;
  }

  micropython_runtime_set_stdout_callback(stdout_capture);
  bool script_ok = false;
  esp_err_t err = micropython_runtime_start(heap, heap_size, source, manifest.source_path, &script_ok);
  if (err != ESP_OK || !script_ok) {
    heap_caps_free(heap);
    show_error_and_reboot(display, "script fail");
    return;
  }

  ESP_LOGI(TAG, "running id=%s heap=%u", manifest.id, static_cast<unsigned>(heap_size));
  render_terminal(display);

  bool exiting = false;
  while (!exiting) {
    AppEvent event {};
    while (xQueueReceive(app_events, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (event.is_key && event.key.pressed) {
        const KeyDef& def = key_at(event.key.row, event.key.col);
        const char* token = key_input(def, event.key.shift, event.key.alpha);
        if (token != nullptr) {
          if (std::strcmp(token, ":app.exit") == 0) {
            exiting = true;
            break;
          }
          micropython_runtime_on_key(token);
        } else if (def.role == KeyRole::Clear) {
          exiting = true;
          break;
        }
      }
    }
    render_terminal(display);
  }

  micropython_runtime_stop();
  heap_caps_free(heap);
  clear_key_overrides();

  ESP_LOGI(TAG, "exit, rebooting to calculator");
  boot::set_mode(boot::Mode::Calculator);
  vTaskDelay(pdMS_TO_TICKS(200));
  esp_restart();
}

}  // namespace

void run_mpy_mode(StorageManager& storage,
                  Weact213BwDisplay& display,
                  QueueHandle_t app_events) {
  ESP_LOGI(TAG, "starting micropython mode");

  const char* app_id = boot::pending_app_id();
  if (app_id[0] == '\0') {
    show_error_and_reboot(display, "no app id");
    return;
  }

  for (size_t i = 0; i < storage.app_count(); ++i) {
    if (std::strcmp(storage.app(i).id, app_id) == 0) {
      ESP_LOGI(TAG, "launch app=%s path=%s", app_id, storage.app(i).source_path);
      run_app(storage, display, app_events, storage.app(i));
      return;
    }
  }

  show_error_and_reboot(display, "app not found");
}

}  // namespace esp32calc_alt
