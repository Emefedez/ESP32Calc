#include "graphics/raylib_epaper_port.h"

#if ESP32CALC_USE_RAYLIB

#include <algorithm>
#include <cstring>

#include "app_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_raylib_port.h"
#include "esp_timer.h"
#include "graphics/mono_canvas.h"
#include "raylib.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "raylib_epd";
constexpr uint8_t kBlackThreshold = 150;

struct EpaperPanelIo {
  esp_lcd_panel_io_t base {};
  esp_lcd_panel_io_callbacks_t callbacks {};
  void* user_ctx = nullptr;
};

struct EpaperPanel {
  esp_lcd_panel_t base {};
  Weact213BwDisplay* display = nullptr;
  EpaperPanelIo* io = nullptr;
  MonoCanvas canvas {};
  RefreshMode next_refresh_mode = RefreshMode::Full;
};

EpaperPanel g_panel;
EpaperPanelIo g_io;

EpaperPanel* panel_from_base(esp_lcd_panel_t* panel) {
  return reinterpret_cast<EpaperPanel*>(panel);
}

EpaperPanelIo* io_from_base(esp_lcd_panel_io_t* io) {
  return reinterpret_cast<EpaperPanelIo*>(io);
}

static inline bool rgb565_is_black(uint16_t pixel) {
  if (pixel == 0xFFFF) return false;
  if (pixel == 0x0000) return true;
  const uint8_t r5 = (pixel >> 11) & 0x1F;
  const uint8_t g6 = (pixel >> 5) & 0x3F;
  const uint8_t b5 = pixel & 0x1F;
  return static_cast<uint32_t>(r5) * 2460u +
         static_cast<uint32_t>(g6) * 2376u +
         static_cast<uint32_t>(b5) * 938u < 150000u;
}

void signal_transfer_done(EpaperPanelIo* io) {
  if (io == nullptr || io->callbacks.on_color_trans_done == nullptr) {
    return;
  }

  esp_lcd_panel_io_event_data_t event {};
  io->callbacks.on_color_trans_done(&io->base, &event, io->user_ctx);
}

esp_err_t panel_noop(esp_lcd_panel_t*) {
  return ESP_OK;
}

esp_err_t panel_draw_bitmap(esp_lcd_panel_t* panel_base,
                            int x_start,
                            int y_start,
                            int x_end,
                            int y_end,
                            const void* color_data) {
  EpaperPanel* panel = panel_from_base(panel_base);
  ESP_RETURN_ON_FALSE(panel != nullptr && panel->display != nullptr,
                      ESP_ERR_INVALID_STATE,
                      TAG,
                      "panel not initialized");
  ESP_RETURN_ON_FALSE(color_data != nullptr, ESP_ERR_INVALID_ARG, TAG, "missing color data");

  const int width = x_end - x_start;
  const int height = y_end - y_start;
  ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "empty bitmap");

  if (x_start == 0 && y_start == 0) {
    panel->canvas.clear(true);
  }

  const int64_t t_pixel_start = esp_timer_get_time();
  const auto* pixels = static_cast<const uint16_t*>(color_data);
  if (x_start == 0 && y_start == 0 &&
      x_end == MonoCanvas::kWidth && y_end == MonoCanvas::kHeight) {
    uint8_t* canvas_data = panel->canvas.data();
    for (int local_y = 0; local_y < height; ++local_y) {
      const uint16_t* src_row = pixels + local_y * width;
      for (int bx = 0; bx < width; bx += 8) {
        uint8_t byte = 0xFF;
        for (int b = 0; b < 8; ++b) {
          if (rgb565_is_black(src_row[bx + b])) {
            byte &= static_cast<uint8_t>(~(0x80 >> b));
          }
        }
        canvas_data[local_y * ((MonoCanvas::kWidth + 7) / 8) + bx / 8] = byte;
      }
    }
  } else {
    for (int local_y = 0; local_y < height; ++local_y) {
      const int dst_y = y_start + local_y;
      if (dst_y < 0 || dst_y >= MonoCanvas::kHeight) {
        continue;
      }
      for (int local_x = 0; local_x < width; ++local_x) {
        const int dst_x = x_start + local_x;
        if (dst_x < 0 || dst_x >= MonoCanvas::kWidth) {
          continue;
        }
        panel->canvas.set_pixel(dst_x, dst_y, rgb565_is_black(pixels[local_y * width + local_x]));
      }
    }
  }
  const int64_t t_pixel_end = esp_timer_get_time();

  if (x_start <= 0 && x_end >= MonoCanvas::kWidth && y_end >= MonoCanvas::kHeight) {
    const RefreshMode mode = panel->next_refresh_mode;
    panel->next_refresh_mode = RefreshMode::Full;
    ESP32CALC_LOGI(TAG, "raylib RGB565 frame -> e-paper %s update (pixel_loop=%lldus)",
                   mode == RefreshMode::Full ? "full" : "fast",
                   t_pixel_end - t_pixel_start);
    const int64_t t_upd_start = esp_timer_get_time();
    const esp_err_t err = panel->display->update_canvas(panel->canvas, mode);
    const int64_t t_upd_end = esp_timer_get_time();
    ESP32CALC_LOGI(TAG, "update_canvas=%lldus (total frame=%lldus)",
                   t_upd_end - t_upd_start,
                   t_upd_end - t_pixel_start);
    signal_transfer_done(panel->io);
    ESP_RETURN_ON_ERROR(err, TAG, "display raylib frame");
    return ESP_OK;
  }

  signal_transfer_done(panel->io);
  return ESP_OK;
}

esp_err_t panel_draw_bitmap_2d(esp_lcd_panel_t* panel,
                               int x_start,
                               int y_start,
                               int x_end,
                               int y_end,
                               const void* src_data,
                               size_t src_x_size,
                               size_t,
                               int src_x_start,
                               int src_y_start,
                               int,
                               int) {
  if (src_x_start == 0 && src_y_start == 0 &&
      src_x_size == static_cast<size_t>(x_end - x_start)) {
    return panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, src_data);
  }
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t panel_bool_not_supported(esp_lcd_panel_t*, bool) {
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t panel_mirror_not_supported(esp_lcd_panel_t*, bool, bool) {
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t panel_gap_not_supported(esp_lcd_panel_t*, int, int) {
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t panel_brightness_not_supported(esp_lcd_panel_t*, int) {
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t io_rx_not_supported(esp_lcd_panel_io_t*, int, void*, size_t) {
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t io_tx_param_noop(esp_lcd_panel_io_t*, int, const void*, size_t) {
  return ESP_OK;
}

esp_err_t io_tx_color_noop(esp_lcd_panel_io_t*, int, const void*, size_t) {
  return ESP_OK;
}

esp_err_t io_del_noop(esp_lcd_panel_io_t*) {
  return ESP_OK;
}

esp_err_t io_register_callbacks(esp_lcd_panel_io_t* io_base,
                                const esp_lcd_panel_io_callbacks_t* callbacks,
                                void* user_ctx) {
  EpaperPanelIo* io = io_from_base(io_base);
  ESP_RETURN_ON_FALSE(io != nullptr, ESP_ERR_INVALID_ARG, TAG, "missing io");
  if (callbacks != nullptr) {
    io->callbacks = *callbacks;
  } else {
    std::memset(&io->callbacks, 0, sizeof(io->callbacks));
  }
  io->user_ctx = user_ctx;
  return ESP_OK;
}

void configure_fake_panel(Weact213BwDisplay& display) {
  g_io = {};
  g_io.base.rx_param = &io_rx_not_supported;
  g_io.base.tx_param = &io_tx_param_noop;
  g_io.base.tx_color = &io_tx_color_noop;
  g_io.base.del = &io_del_noop;
  g_io.base.register_event_callbacks = &io_register_callbacks;

  g_panel = {};
  g_panel.base.reset = &panel_noop;
  g_panel.base.init = &panel_noop;
  g_panel.base.del = &panel_noop;
  g_panel.base.draw_bitmap = &panel_draw_bitmap;
  g_panel.base.draw_bitmap_2d = &panel_draw_bitmap_2d;
  g_panel.base.mirror = &panel_mirror_not_supported;
  g_panel.base.swap_xy = &panel_bool_not_supported;
  g_panel.base.set_gap = &panel_gap_not_supported;
  g_panel.base.invert_color = &panel_bool_not_supported;
  g_panel.base.disp_on_off = &panel_bool_not_supported;
  g_panel.base.disp_sleep = &panel_bool_not_supported;
  g_panel.base.set_brightness = &panel_brightness_not_supported;
  g_panel.display = &display;
  g_panel.io = &g_io;
}
}  // namespace

void RaylibEpaperPort::set_next_refresh_mode(RefreshMode mode) {
  g_panel.next_refresh_mode = mode;
}

esp_err_t RaylibEpaperPort::init(Weact213BwDisplay& display) {
  if (initialized_) {
    return ESP_OK;
  }

  configure_fake_panel(display);

  ray_port_cfg_t port_cfg {};
  port_cfg.buff_psram = true;
  port_cfg.double_buffer = false;
  port_cfg.buffer_pixels = 0;
  port_cfg.chunk_bytes = config::kDisplayLogicalWidth * 48 * sizeof(uint16_t);
  port_cfg.swap_rgb_bytes = false;
  port_cfg.hres = config::kDisplayLogicalWidth;
  port_cfg.vres = config::kDisplayLogicalHeight;
  port_cfg.rotation = 0;
  port_cfg.perf_stats = true;
  ESP_RETURN_ON_ERROR(ray_port_init(&port_cfg), TAG, "ray port init");

  ray_port_display_cfg_t display_cfg {};
  display_cfg.panel = &g_panel.base;
  display_cfg.io = &g_io.base;
  display_cfg.hres = config::kDisplayLogicalWidth;
  display_cfg.vres = config::kDisplayLogicalHeight;
  display_cfg.monochrome = true;
  display_cfg.dma_capable = false;
  ESP_RETURN_ON_ERROR(ray_port_add_display(&display_cfg), TAG, "ray port display");

  SetTraceLogLevel(LOG_WARNING);
  InitWindow(config::kDisplayLogicalWidth, config::kDisplayLogicalHeight, "ESP32Calc");

  initialized_ = true;
  ESP32CALC_LOGI(TAG, "raylib software renderer ready for e-paper bridge");
  return ESP_OK;
}

}  // namespace esp32calc

#endif  // ESP32CALC_USE_RAYLIB
