#pragma once

#include <cstddef>
#include <cstdint>

#include "app_events.h"
#include "battery/battery_monitor.h"
#include "display/weact_213_bw.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "graphics/mono_canvas.h"
#include "ui/modes/mode_registry.h"

namespace esp32calc {

class MenuUi {
 public:
  MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display);
  ~MenuUi();
  [[noreturn]] void run();

 private:
  enum class Screen : uint8_t {
    Menu,
    Mode,
  };

  void apply_key(const KeyEvent& key);
  void update_state(const AppEvent& event);
  void render();

  void render_status_bar();
  void render_menu();
  void render_content();

  void open_selected_mode();
  void close_active_mode();
  void move_selection(int delta);

  static constexpr int kStatusBarHeight = 14;

  QueueHandle_t app_events_;
  Weact213BwDisplay& display_;
  MonoCanvas canvas_ {};
  BatterySnapshot battery_ {};
  const ModeDescriptorList& modes_;
  alignas(kModeStorageAlign) std::byte mode_storage_[kModeStorageSize] {};
  UiMode* active_mode_ = nullptr;
  ModeDestroyFn active_mode_destroy_ = nullptr;
  Screen screen_ = Screen::Menu;
  uint8_t selected_ = 0;
  bool shift_ = false;
  bool alpha_ = false;
  bool first_render_done_ = false;
  bool full_refresh_pending_ = true;
};

}  // namespace esp32calc
