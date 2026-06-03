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
#include "system/chatbot_service.h"
#include "system/storage_manager.h"
#include "ui/menu_constants.h"

namespace esp32calc_alt {

class MenuMode;
class StandardMenuMode;
class GraphViewMode;
class SolverMenuMode;
class MatrixMenuMode;
class ConstantsMenuMode;
class IntegralsMenuMode;
class AppsMenuMode;

class MenuUi {
 public:
  MenuUi(QueueHandle_t app_events,
         Weact213BwDisplay& display,
         MathService& math,
         StorageManager& storage,
         ChatbotService& chatbot);
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
    Solver,
    Matrix,
    Constants,
    Integrals,
    Apps,
  };

  enum class IntegralMenuStage : uint8_t {
    Groups,
    Items,
  };

  enum class ConstantMenuStage : uint8_t {
    Groups,
    Items,
  };

  enum class SolverMenuStage : uint8_t {
    Groups,
    Items,
  };

  enum class MatrixMenuStage : uint8_t {
    Matrices,
    Size,
    Cells,
  };

  enum class AppMenuStage : uint8_t {
    List,
    ChatQuestion,
    ChatWaiting,
    ChatAnswer,
  };

  enum class VariablePalette : uint8_t {
    None,
    Plain,
    Square,
  };

  struct GraphCacheEntry {
    // Stores a Giac-sampled x-range, often wider than current viewport. Pan can
    // interpolate from this instead of queuing another CAS graph job.
    bool used = false;
    char expression[menu_constants::kGraphExpressionCapacity] {};
    float x_min = 0.0f;
    float x_max = 0.0f;
    size_t count = 0;
    uint32_t age = 0;
    float y[kGraphSampleCount] {};
    bool valid[kGraphSampleCount] {};
  };

  static constexpr size_t kGraphPsramCacheEntries = 8;
  static constexpr size_t kGraphFallbackCacheEntries = 2;

  void update_from_key(const KeyEvent& key);
  void apply_selector_key(const KeyEvent& key, const KeyDef& def);
  void apply_standard_key(const KeyEvent& key);
  void apply_graph_key(const KeyEvent& key);
  void apply_graph_result(const MathResult& result);
  void apply_solver_key(const KeyEvent& key);
  void apply_matrix_key(const KeyEvent& key);
  void apply_constants_key(const KeyEvent& key);
  void apply_integrals_key(const KeyEvent& key);
  void apply_apps_key(const KeyEvent& key);
  void apply_math_result(const MathResult& result);
  void apply_chatbot_result(const ChatbotResult& result);
  void consume_modifiers();
  ModeKind mode_from_index(uint8_t index) const;
  uint8_t index_from_mode(ModeKind kind) const;
  void open_mode(ModeKind kind);
  void close_active_mode();
  void move_mode_selection(int delta);
  bool open_app_by_id(const char* app_id);
  void open_selected_app();
  void return_to_app_list();
  bool app_command(const char* token);

  bool append_expression(const char* token);
  bool append_expression_at_cursor(const char* token, size_t token_cursor);
  bool insert_fraction_template();
  void delete_expression_char();
  void clear_expression();
  void clear_result();
  void submit_expression(bool decimal_output = false);
  void open_graph_from_expression();
  void open_graph_expression(const char* expression);
  void queue_graph_sample();
  void pan_graph(float dx_fraction, float dy_fraction);
  void zoom_graph(float factor);
  bool restore_graph_cache(const char* expression);
  void store_graph_cache(const MathResult& result);
  bool graph_view_inside_sample(float sample_min, float sample_max) const;
  bool copy_graph_view_from_series(const float* y,
                                   const bool* valid,
                                   size_t count,
                                   float sample_min,
                                   float sample_max);
  void fit_graph_y_to_visible_values();
  void move_matrix_selection(int delta);
  void move_matrix_cell(int row_delta, int col_delta);
  void adjust_matrix_rows(int delta);
  void adjust_matrix_cols(int delta);
  void fill_matrix_zero(uint8_t matrix);
  void fill_matrix_identity(uint8_t matrix);
  bool append_matrix_cell_token(const char* token);
  void delete_matrix_cell_char();
  bool build_matrix_assignment(uint8_t matrix, char* output, size_t output_size) const;
  void commit_matrix_definition();
  void insert_matrix_name();
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
  void open_solver_group(uint8_t group);
  void choose_selected_solver();
  void move_solver_group_selection(int delta);
  void move_solver_item_selection(int delta);
  void clear_solver_search();
  void backspace_solver_search();
  void append_solver_search_token(const char* token);
  void sync_solver_selection_to_filter();
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
  void append_chatbot_question_token(const char* token);
  void delete_chatbot_question_char();
  void submit_chatbot_question();

  void render();
  void render_status_bar();
  void render_mode_selector();
  void render_standard();
  void render_graph();
  void render_solver();
  void render_matrix();
  void render_constants();
  void render_integrals();
  void render_apps();
  void render_variable_palette();

  friend class StandardMenuMode;
  friend class GraphViewMode;
  friend class SolverMenuMode;
  friend class MatrixMenuMode;
  friend class ConstantsMenuMode;
  friend class IntegralsMenuMode;
  friend class AppsMenuMode;

  QueueHandle_t app_events_;
  Weact213BwDisplay& display_;
  MathService& math_;
  StorageManager& storage_;
  ChatbotService& chatbot_;
  MonoCanvas canvas_ {};
  // One active mode object, placement-new into fixed storage. Menus do not all
  // stay live, matching the active-only memory policy.
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
  bool graph_auto_y_ = true;
  bool graph_has_result_ = false;
  bool graph_has_error_ = false;
  bool graph_show_numbers_ = false;
  GraphCacheEntry* graph_cache_ = nullptr;
  size_t graph_cache_capacity_ = 0;
  uint32_t graph_cache_age_ = 0;
  // Internal fallback keeps Wokwi/no-PSRAM builds functional; PSRAM path gives
  // more graph history without taking internal RAM.
  GraphCacheEntry graph_cache_fallback_[kGraphFallbackCacheEntries] {};
  MatrixMenuStage matrix_stage_ = MatrixMenuStage::Matrices;
  uint8_t matrix_selected_ = 0;
  uint8_t matrix_cell_row_ = 0;
  uint8_t matrix_cell_col_ = 0;
  bool matrix_cell_editing_ = false;
  uint8_t matrix_rows_[menu_constants::kMatrixCount] {2, 2, 2, 2, 2, 2};
  uint8_t matrix_cols_[menu_constants::kMatrixCount] {2, 2, 2, 2, 2, 2};
  // Matrix editor keeps tiny fixed cell text buffers. Giac receives a bounded
  // assignment string on save; no dynamic matrix UI storage.
  char matrix_cells_[menu_constants::kMatrixCount]
                    [menu_constants::kMatrixMaxRows]
                    [menu_constants::kMatrixMaxCols]
                    [menu_constants::kMatrixCellCapacity] {};
  char result_text_[menu_constants::kResultCapacity] {};
  bool result_visible_ = false;
  bool result_is_error_ = false;
  bool result_decimal_ = false;
  BatterySnapshot battery_ {};
  const char* status_ = "ENTER SENDS";
  VariablePalette variable_palette_ = VariablePalette::None;
  uint8_t variable_selected_ = 0;
  ConstantMenuStage constant_stage_ = ConstantMenuStage::Groups;
  uint8_t constant_group_selected_ = 0;
  uint8_t constant_selected_ = 0;
  char constant_search_[12] {};
  SolverMenuStage solver_stage_ = SolverMenuStage::Groups;
  uint8_t solver_group_selected_ = 0;
  uint8_t solver_selected_ = 0;
  char solver_search_[12] {};
  IntegralMenuStage integral_stage_ = IntegralMenuStage::Groups;
  uint8_t integral_group_selected_ = 0;
  uint8_t integral_selected_ = 0;
  char integral_search_[12] {};
  AppMenuStage app_stage_ = AppMenuStage::List;
  uint8_t app_selected_ = 0;
  uint8_t active_app_index_ = 0;
  char chatbot_question_[128] {};
  char chatbot_answer_[192] {};
  bool chatbot_answer_error_ = false;
};

}  // namespace esp32calc_alt
