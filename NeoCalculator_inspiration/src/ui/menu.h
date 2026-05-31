#pragma once

#include <cstddef>
#include <cstdint>

#include "app_events.h"
#include "display/weact_213_bw.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "graphics/mono_canvas.h"
#include "math/math_service.h"

namespace esp32calc_alt {

class MenuUi {
 public:
  MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display, MathService& math);
  [[noreturn]] void run();

 private:
  enum class Screen : uint8_t {
    Standard,
    Graph,
  };

  enum class VariablePalette : uint8_t {
    None,
    Plain,
    Square,
  };

  void update_from_key(const KeyEvent& key);
  void apply_standard_key(const KeyEvent& key);
  void apply_graph_key(const KeyEvent& key);
  void apply_math_result(const MathResult& result);
  void consume_modifiers();

  bool append_expression(const char* token);
  void delete_expression_char();
  void clear_expression();
  void clear_result();
  void submit_expression();
  void open_graph_from_expression();
  void open_variable_palette(VariablePalette palette);
  void handle_variable_palette_key(const KeyEvent& key);
  void choose_selected_variable();
  void move_variable_selection(int delta);
  void move_cursor_left(bool all_the_way);
  void move_cursor_right(bool all_the_way);
  size_t expression_visible_start() const;
  bool key_is_equals(const KeyEvent& key) const;

  void render();
  void render_status_bar();
  void render_standard();
  void render_graph();
  void render_variable_palette();

  static constexpr size_t kExpressionCapacity = 128;
  static constexpr size_t kResultCapacity = 192;
  static constexpr size_t kVisibleExpressionChars = 38;
  static constexpr size_t kVisibleResultChars = 36;

  QueueHandle_t app_events_;
  Weact213BwDisplay& display_;
  MathService& math_;
  MonoCanvas canvas_ {};
  Screen screen_ = Screen::Standard;
  bool shift_ = false;
  bool alpha_ = false;
  bool first_render_done_ = false;
  bool full_refresh_pending_ = true;
  char expression_[kExpressionCapacity] {};
  size_t expression_len_ = 0;
  size_t cursor_ = 0;
  char result_text_[kResultCapacity] {};
  bool result_visible_ = false;
  bool result_is_error_ = false;
  const char* status_ = "ENTER SENDS";
  VariablePalette variable_palette_ = VariablePalette::None;
  uint8_t variable_selected_ = 0;
};

}  // namespace esp32calc_alt
