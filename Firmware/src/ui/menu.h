#pragma once

#include <array>
#include <cstdint>

#include "app_events.h"
#include "battery/battery_monitor.h"
#include "display/weact_213_bw.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "graphics/mono_canvas.h"
#if ESP32CALC_USE_RAYLIB
#include "graphics/raylib_epaper_port.h"
#endif

namespace esp32calc {

class MenuUi {
 public:
  MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display);
  [[noreturn]] void run();

 private:
  enum class Screen : uint8_t {
    Menu,
    Standard,
    Graph,
    Symbolic,
    Programs,
    Files,
    Wireless,
    Settings,
  };

  void apply_key(const KeyEvent& key);
  void update_state(const AppEvent& event);
  void handle_event(const AppEvent& event);
  void handle_key(const KeyEvent& key);
  void render(RefreshMode mode);
#if ESP32CALC_USE_RAYLIB
  void render_raylib(RefreshMode mode);
  void render_status_bar_raylib();
  void render_menu_raylib();
  void render_placeholder_raylib(const char* title, const char* subtitle);
#endif
  void render_status_bar();
  void render_menu();
  void render_placeholder(const char* title, const char* subtitle);
  void open_selected_mode();
  void move_selection(int delta);
  Screen screen_for_index(uint8_t index) const;

  QueueHandle_t app_events_;
  Weact213BwDisplay& display_;
#if ESP32CALC_USE_RAYLIB
  RaylibEpaperPort raylib_;
#endif
  MonoCanvas canvas_ {};
  BatterySnapshot battery_ {};
  Screen screen_ = Screen::Menu;
  uint8_t selected_ = 0;
  bool shift_ = false;
  bool alpha_ = false;
  bool first_render_done_ = false;
};

}  // namespace esp32calc
