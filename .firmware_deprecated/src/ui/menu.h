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
  ~MenuUi(); // ~ means destructor, this exists because only one mode needs to be active at a given time, so without it we would be wasting memory
  [[noreturn]] void run(); // this is in the main loop, thus, [[noreturn]] is used

 private:
  enum class Screen : uint8_t { // This may be stupid
    Menu,
    Mode,
  };

  // while the inside of most of these functions is kind of spaguetti rn, I like the "make the main loop look as much as pseudo-code as possible" approach, feels quite easy to expand and to read overall, even if diving deeper can suck.

  void apply_key(const KeyEvent& key);
  void apply_calc_result(CalcResult* result);
  void apply_mode_result(ModeResult result);
  void consume_input_modifiers();
  void update_state(AppEvent& event);
  void render();

  void render_status_bar();
  void render_menu();
  void render_content();

  bool open_mode_by_label(const char* label); // easier to use from Menu, boolean so it returns false if it fails. 
  void open_mode(const ModeDescriptor& entry); // easier to use when calling from code itself, this probably should be boolean too.
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
