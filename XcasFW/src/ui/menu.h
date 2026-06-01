#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

#include "app_events.h"
#include "display/weact_213_bw.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "graphics/mono_canvas.h"
#include "hardware/keymap.h"
#include "math/math_service.h"
#include "ui/menu_constants.h"

namespace esp32calc_alt {

class MenuMode;
class StandardMenuMode;
class GraphMenuMode;
class ConstantsMenuMode;

class MenuUi {
 public:
  MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display, MathService& math);
  ~MenuUi();
  [[noreturn]] void run();

 private:
  enum class Screen : uint8_t {
    ModeSelector,
    Mode,
  };

  enum class ModeKind : uint8_t {
    Standard,
    Graph,
    Constants,
  };

  enum class VariablePalette : uint8_t {
    None,
    Plain,
    Square,
  };

  void update_from_key(const KeyEvent& key);
  void apply_selector_key(const KeyEvent& key, const KeyDef& def);
  void apply_standard_key(const KeyEvent& key);
  void apply_graph_key(const KeyEvent& key);
  void apply_constants_key(const KeyEvent& key);
  void apply_math_result(const MathResult& result);
  void consume_modifiers();
  void open_mode(ModeKind kind);
  void close_active_mode();
  void move_mode_selection(int delta);

  bool append_expression(const char* token);
  void delete_expression_char();
  void clear_expression();
  void clear_result();
  void submit_expression();
  void open_graph_from_expression();
  void open_graph_expression(const char* expression);
  void open_variable_palette(VariablePalette palette);
  void handle_variable_palette_key(const KeyEvent& key);
  void choose_selected_variable();
  void move_variable_selection(int delta);
  void choose_selected_constant();
  void move_constant_selection(int delta);
  void move_cursor_left(bool all_the_way);
  void move_cursor_right(bool all_the_way);
  size_t expression_visible_start() const;
  bool key_is_equals(const KeyEvent& key) const;

  void render();
  void render_status_bar();
  void render_mode_selector();
  void render_standard();
  void render_graph();
  void render_constants();
  void render_variable_palette();

  friend class StandardMenuMode;
  friend class GraphMenuMode;
  friend class ConstantsMenuMode;

  QueueHandle_t app_events_;
  Weact213BwDisplay& display_;
  MathService& math_;
  MonoCanvas canvas_ {};
  alignas(menu_constants::kModeStorageAlign) std::byte mode_storage_[menu_constants::kModeStorageSize] {};
  MenuMode* active_mode_ = nullptr;
  ModeKind active_mode_kind_ = ModeKind::Standard;
  uint8_t selected_mode_ = 0;
  Screen screen_ = Screen::Mode;
  bool shift_ = false;
  bool alpha_ = false;
  bool first_render_done_ = false;
  bool full_refresh_pending_ = true;
  char expression_[menu_constants::kExpressionCapacity] {};
  size_t expression_len_ = 0;
  size_t cursor_ = 0;
  char graph_expression_[menu_constants::kExpressionCapacity] {"sin(x)"};
  char result_text_[menu_constants::kResultCapacity] {};
  bool result_visible_ = false;
  bool result_is_error_ = false;
  const char* status_ = "ENTER SENDS";
  VariablePalette variable_palette_ = VariablePalette::None;
  uint8_t variable_selected_ = 0;
  uint8_t constant_selected_ = 0;
};

}  // namespace esp32calc_alt
