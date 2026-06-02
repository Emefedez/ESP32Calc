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
class IntegralsMenuMode;

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
    Integrals,
  };

  enum class IntegralMenuStage : uint8_t {
    Groups,
    Items,
  };

  enum class ConstantMenuStage : uint8_t {
    Groups,
    Items,
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
  void apply_graph_result(const MathResult& result);
  void apply_constants_key(const KeyEvent& key);
  void apply_integrals_key(const KeyEvent& key);
  void apply_math_result(const MathResult& result);
  void consume_modifiers();
  ModeKind mode_from_index(uint8_t index) const;
  uint8_t index_from_mode(ModeKind kind) const;
  void open_mode(ModeKind kind);
  void close_active_mode();
  void move_mode_selection(int delta);

  bool append_expression(const char* token);
  bool append_expression_at_cursor(const char* token, size_t token_cursor);
  bool insert_fraction_template();
  void delete_expression_char();
  void clear_expression();
  void clear_result();
  void submit_expression();
  void open_graph_from_expression();
  void open_graph_expression(const char* expression);
  void queue_graph_sample();
  void pan_graph(float dx_fraction, float dy_fraction);
  void zoom_graph(float factor);
  void open_variable_palette(VariablePalette palette);
  void handle_variable_palette_key(const KeyEvent& key);
  void choose_selected_variable();
  void move_variable_selection(int delta);
  void open_constant_group(uint8_t group);
  void choose_selected_constant();
  void move_constant_group_selection(int delta);
  void move_constant_item_selection(int delta);
  void clear_constant_search();
  void backspace_constant_search();
  void append_constant_search_token(const char* token);
  void sync_constant_selection_to_filter();
  void open_integral_group(uint8_t group);
  void choose_selected_integral();
  void move_integral_group_selection(int delta);
  void move_integral_item_selection(int delta);
  void clear_integral_search();
  void backspace_integral_search();
  void append_integral_search_token(const char* token);
  void sync_integral_selection_to_filter();
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
  void render_integrals();
  void render_variable_palette();

  friend class StandardMenuMode;
  friend class GraphMenuMode;
  friend class ConstantsMenuMode;
  friend class IntegralsMenuMode;

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
  bool cursor_visible_ = true;
  bool first_render_done_ = false;
  bool full_refresh_pending_ = true;
  char expression_[menu_constants::kExpressionCapacity] {};
  size_t expression_len_ = 0;
  size_t cursor_ = 0;
  char graph_expression_[menu_constants::kExpressionCapacity] {"sin(x)"};
  float graph_y_[kGraphSampleCount] {};
  bool graph_valid_[kGraphSampleCount] {};
  size_t graph_count_ = 0;
  float graph_x_min_ = -5.0f;
  float graph_x_max_ = 5.0f;
  float graph_y_min_ = -5.0f;
  float graph_y_max_ = 5.0f;
  bool graph_has_result_ = false;
  bool graph_has_error_ = false;
  bool graph_show_numbers_ = false;
  char result_text_[menu_constants::kResultCapacity] {};
  bool result_visible_ = false;
  bool result_is_error_ = false;
  BatterySnapshot battery_ {};
  const char* status_ = "ENTER SENDS";
  VariablePalette variable_palette_ = VariablePalette::None;
  uint8_t variable_selected_ = 0;
  ConstantMenuStage constant_stage_ = ConstantMenuStage::Groups;
  uint8_t constant_group_selected_ = 0;
  uint8_t constant_selected_ = 0;
  char constant_search_[12] {};
  IntegralMenuStage integral_stage_ = IntegralMenuStage::Groups;
  uint8_t integral_group_selected_ = 0;
  uint8_t integral_selected_ = 0;
  char integral_search_[12] {};
};

}  // namespace esp32calc_alt
