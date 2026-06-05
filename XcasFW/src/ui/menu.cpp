#include "ui/menu.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

#include "app_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "hardware/keymap.h"
#include "ui/menu_constants.h"

namespace esp32calc_alt {
namespace {

namespace constants = menu_constants;
constexpr TickType_t kCursorBlinkTicks = pdMS_TO_TICKS(650);

}  // namespace

class MenuMode {
 public:
  virtual ~MenuMode() = default;
  virtual const char* name() const = 0;
  virtual void handle_key(const KeyEvent& key) = 0;
  virtual void handle_math_result(const MathResult& result) {
    (void)result;
  }
  virtual void render(MonoCanvas& canvas) = 0;
};

class StandardMenuMode final : public MenuMode {
 public:
  explicit StandardMenuMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "STANDARD"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_standard_key(key); }
  void handle_math_result(const MathResult& result) override { owner_.apply_math_result(result); }
  void render(MonoCanvas&) override { owner_.render_standard(); }

 private:
  MenuUi& owner_;
};

class GraphViewMode final : public MenuMode {
 public:
  explicit GraphViewMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "GRAPH VIEW"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_graph_key(key); }
  void handle_math_result(const MathResult& result) override { owner_.apply_graph_result(result); }
  void render(MonoCanvas&) override { owner_.render_graph(); }

 private:
  MenuUi& owner_;
};

class SolverMenuMode final : public MenuMode {
 public:
  explicit SolverMenuMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "SOLVER"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_solver_key(key); }
  void render(MonoCanvas&) override { owner_.render_solver(); }

 private:
  MenuUi& owner_;
};

class MatrixMenuMode final : public MenuMode {
 public:
  explicit MatrixMenuMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "MATRIX"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_matrix_key(key); }
  void render(MonoCanvas&) override { owner_.render_matrix(); }

 private:
  MenuUi& owner_;
};

class ConstantsMenuMode final : public MenuMode {
 public:
  explicit ConstantsMenuMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "CONST"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_constants_key(key); }
  void render(MonoCanvas&) override { owner_.render_constants(); }

 private:
  MenuUi& owner_;
};

class IntegralsMenuMode final : public MenuMode {
 public:
  explicit IntegralsMenuMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "CALCULUS"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_integrals_key(key); }
  void render(MonoCanvas&) override { owner_.render_integrals(); }

 private:
  MenuUi& owner_;
};

class AppsMenuMode final : public MenuMode {
 public:
  explicit AppsMenuMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "APPS"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_apps_key(key); }
  void render(MonoCanvas&) override { owner_.render_apps(); }

 private:
  MenuUi& owner_;
};

MenuUi::MenuUi(QueueHandle_t app_events,
               Weact213BwDisplay& display,
               MathService& math,
               StorageManager& storage,
               WifiManager& wifi)
    : app_events_(app_events),
      display_(display),
      math_(math),
      storage_(storage),
      wifi_(wifi) {
  // Graph cache is the one intentionally large UI allocation. Prefer PSRAM;
  // fall back to two internal entries so Wokwi/no-PSRAM still behaves.
  graph_cache_ = static_cast<GraphCacheEntry*>(
      heap_caps_calloc(kGraphPsramCacheEntries,
                       sizeof(GraphCacheEntry),
                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (graph_cache_ == nullptr) {
    graph_cache_ = graph_cache_fallback_;
    graph_cache_capacity_ = kGraphFallbackCacheEntries;
    ESP_LOGW(constants::kLogTag, "graph cache using internal fallback");
  } else {
    graph_cache_capacity_ = kGraphPsramCacheEntries;
    ESP_LOGI(constants::kLogTag,
             "graph cache in psram: %u bytes",
             static_cast<unsigned>(kGraphPsramCacheEntries * sizeof(GraphCacheEntry)));
  }
  open_mode(ModeKind::Standard);
}

MenuUi::~MenuUi() {
  close_active_mode();
  // Only free heap allocation; fallback array is part of MenuUi storage.
  if (graph_cache_ != nullptr && graph_cache_ != graph_cache_fallback_) {
    heap_caps_free(graph_cache_);
  }
}

void MenuUi::run() {
  ESP_LOGI(constants::kLogTag, "starting alt menu");
  render();
  TickType_t last_cursor_toggle = xTaskGetTickCount();

  while (true) {
    AppEvent event {};
    if (xQueueReceive(app_events_, &event, pdMS_TO_TICKS(25)) == pdTRUE) {
      if (event.is_key) {
        update_from_key(event.key);
        last_cursor_toggle = xTaskGetTickCount();
      } else {
        battery_ = event.battery;
      }
      while (xQueueReceive(app_events_, &event, 0) == pdTRUE) {
        if (event.is_key) {
          update_from_key(event.key);
          last_cursor_toggle = xTaskGetTickCount();
        } else {
          battery_ = event.battery;
        }
      }
      render();
    }

    MathResult result {};
    if (math_.poll_result(result, 0)) {
      if (screen_ == Screen::Mode && active_mode_ != nullptr) {
        active_mode_->handle_math_result(result);
      }
      render();
    }

    const TickType_t now = xTaskGetTickCount();
    if (screen_ == Screen::Mode &&
        (active_mode_kind_ == ModeKind::Standard ||
         (active_mode_kind_ == ModeKind::Matrix && matrix_cell_editing_)) &&
        now - last_cursor_toggle >= kCursorBlinkTicks) {
      cursor_visible_ = !cursor_visible_;
      last_cursor_toggle = now;
      render();
    }
  }
}

void MenuUi::update_from_key(const KeyEvent& key) {
  if (!key.pressed) {
    return;
  }

  cursor_visible_ = true;
  const KeyDef& def = key_at(key.row, key.col);
  if (is_blank_key(def)) {
    return;
  }

  if (def.role == KeyRole::Shift) {
    shift_ = !shift_;
    return;
  }
  if (def.role == KeyRole::Alpha) {
    alpha_ = !alpha_;
    return;
  }

  KeyEvent modified = key;
  modified.shift = shift_;
  modified.alpha = alpha_;

  if (def.role == KeyRole::Mode) {
    if (screen_ == Screen::ModeSelector) {
      open_mode(mode_from_index(selected_mode_));
    } else {
      close_active_mode();
      screen_ = Screen::ModeSelector;
      status_ = "PICK MENU";
      full_refresh_pending_ = true;
    }
    consume_modifiers();
    return;
  }

  if (screen_ == Screen::ModeSelector) {
    apply_selector_key(modified, def);
  } else if (active_mode_ != nullptr) {
    active_mode_->handle_key(modified);
  } else {
    screen_ = Screen::ModeSelector;
    full_refresh_pending_ = true;
  }

  consume_modifiers();
}

void MenuUi::apply_selector_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;
  const int digit = key_digit(def);
  if (digit >= 0 && static_cast<size_t>(digit) < constants::kModeCount) {
    selected_mode_ = static_cast<uint8_t>(digit);
    open_mode(mode_from_index(selected_mode_));
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
    case KeyRole::Up:
      move_mode_selection(-1);
      status_ = "PICK MENU";
      return;
    case KeyRole::Right:
    case KeyRole::Down:
      move_mode_selection(1);
      status_ = "PICK MENU";
      return;
    case KeyRole::Enter:
      open_mode(mode_from_index(selected_mode_));
      return;
    case KeyRole::Clear:
      open_mode(active_mode_kind_);
      return;
    default:
      return;
  }
}

void MenuUi::consume_modifiers() {
  shift_ = false;
  alpha_ = false;
}

MenuUi::ModeKind MenuUi::mode_from_index(uint8_t index) const {
  switch (index) {
    case 1:
      return ModeKind::Solver;
    case 2:
      return ModeKind::Matrix;
    case 3:
      return ModeKind::Constants;
    case 4:
      return ModeKind::Integrals;
    case 5:
      return ModeKind::Apps;
    case 0:
    default:
      return ModeKind::Standard;
  }
}

uint8_t MenuUi::index_from_mode(ModeKind kind) const {
  switch (kind) {
    case ModeKind::Graph:
      return 0;
    case ModeKind::Solver:
      return 1;
    case ModeKind::Matrix:
      return 2;
    case ModeKind::Constants:
      return 3;
    case ModeKind::Integrals:
      return 4;
    case ModeKind::Apps:
      return 5;
    case ModeKind::Standard:
    default:
      return 0;
  }
}

void MenuUi::open_mode(ModeKind kind) {
  close_active_mode();
  active_mode_kind_ = kind;
  selected_mode_ = index_from_mode(kind);

  // Rebuild mode object in-place so submenu state stays active-only and cheap.
  switch (kind) {
    case ModeKind::Integrals:
      static_assert(sizeof(IntegralsMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) IntegralsMenuMode(*this);
      integral_stage_ = IntegralMenuStage::Groups;
      clear_integral_search();
      status_ = "PICK GROUP";
      break;
    case ModeKind::Solver:
      static_assert(sizeof(SolverMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) SolverMenuMode(*this);
      solver_stage_ = SolverMenuStage::Groups;
      clear_solver_search();
      status_ = "PICK GROUP";
      break;
    case ModeKind::Constants:
      static_assert(sizeof(ConstantsMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) ConstantsMenuMode(*this);
      constant_stage_ = ConstantMenuStage::Groups;
      clear_constant_search();
      status_ = "PICK GROUP";
      break;
    case ModeKind::Matrix:
      static_assert(sizeof(MatrixMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) MatrixMenuMode(*this);
      matrix_stage_ = MatrixMenuStage::Matrices;
      matrix_cell_row_ = 0;
      matrix_cell_col_ = 0;
      matrix_cell_editing_ = false;
      status_ = "PICK MATRIX";
      break;
    case ModeKind::Graph:
      static_assert(sizeof(GraphViewMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) GraphViewMode(*this);
      status_ = "GRAPH VIEW";
      break;
    case ModeKind::Apps:
      static_assert(sizeof(AppsMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) AppsMenuMode(*this);
      app_stage_ = AppMenuStage::List;
      status_ = storage_.app_count() == 0 ? "NO APPS" : "PICK APP";
      break;
    case ModeKind::Standard:
    default:
      static_assert(sizeof(StandardMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) StandardMenuMode(*this);
      status_ = "ENTER SENDS";
      break;
  }

  screen_ = Screen::Mode;
  full_refresh_pending_ = true;
}

void MenuUi::close_active_mode() {
  if (active_mode_ != nullptr) {
    switch (active_mode_kind_) {
      case ModeKind::Integrals:
        static_cast<IntegralsMenuMode*>(active_mode_)->~IntegralsMenuMode();
        break;
      case ModeKind::Solver:
        static_cast<SolverMenuMode*>(active_mode_)->~SolverMenuMode();
        break;
      case ModeKind::Constants:
        static_cast<ConstantsMenuMode*>(active_mode_)->~ConstantsMenuMode();
        break;
      case ModeKind::Graph:
        static_cast<GraphViewMode*>(active_mode_)->~GraphViewMode();
        break;
      case ModeKind::Apps:
        static_cast<AppsMenuMode*>(active_mode_)->~AppsMenuMode();
        app_runtime_.close();
        clear_key_overrides();
        storage_.apply_global_keymap();
        break;
      case ModeKind::Matrix:
        static_cast<MatrixMenuMode*>(active_mode_)->~MatrixMenuMode();
        break;
      case ModeKind::Standard:
      default:
        static_cast<StandardMenuMode*>(active_mode_)->~StandardMenuMode();
        break;
    }
    active_mode_ = nullptr;
  }
}

void MenuUi::move_mode_selection(int delta) {
  int next = static_cast<int>(selected_mode_) + delta;
  while (next < 0) {
    next += static_cast<int>(constants::kModeCount);
  }
  selected_mode_ = static_cast<uint8_t>(next % static_cast<int>(constants::kModeCount));
}

bool MenuUi::open_app_by_id(const char* app_id) {
  if (app_id == nullptr) {
    return false;
  }
  for (size_t i = 0; i < storage_.app_count(); ++i) {
    if (std::strcmp(storage_.app(i).id, app_id) == 0) {
      open_mode(ModeKind::Apps);
      app_selected_ = static_cast<uint8_t>(i);
      open_selected_app();
      return true;
    }
  }
  status_ = "APP MISSING";
  return false;
}

void MenuUi::open_selected_app() {
  if (storage_.app_count() == 0 || app_selected_ >= storage_.app_count()) {
    status_ = "NO APPS";
    return;
  }
  active_app_index_ = app_selected_;
  const ExternalAppManifest& app = storage_.app(active_app_index_);
  clear_key_overrides();
  storage_.apply_global_keymap();
  storage_.apply_app_keymap(app.id);
  if (app.wants_wifi) {
#if ESP32CALC_ENABLE_WIFI_APPS
    if (!storage_.wifi().loaded) {
      app_stage_ = AppMenuStage::Running;
      status_ = "NO WIFI CFG";
      full_refresh_pending_ = true;
      return;
    }
    const esp_err_t wifi_err = wifi_.start(storage_.wifi());
    if (wifi_err != ESP_OK) {
      app_stage_ = AppMenuStage::Running;
      status_ = "WIFI ERROR";
      full_refresh_pending_ = true;
      return;
    }
#else
    app_stage_ = AppMenuStage::Running;
    status_ = "WIFI OFF";
    full_refresh_pending_ = true;
    return;
#endif
  }
  const esp_err_t err = app_runtime_.open(app);
  app_stage_ = AppMenuStage::Running;
  status_ = err == ESP_OK ? "APP RUN" : "APP ERROR";
  full_refresh_pending_ = true;
}

void MenuUi::return_to_app_list() {
  app_runtime_.close();
  clear_key_overrides();
  storage_.apply_global_keymap();
  app_stage_ = AppMenuStage::List;
  status_ = storage_.app_count() == 0 ? "NO APPS" : "PICK APP";
  full_refresh_pending_ = true;
}

bool MenuUi::app_command(const char* token) {
  if (token == nullptr || std::strncmp(token, ":app.", 5) != 0) {
    return false;
  }
  const char* id = token + 5;
  if (std::strcmp(id, "exit") == 0) {
    return_to_app_list();
    return true;
  }
  open_app_by_id(id);
  return true;
}

void MenuUi::render() {
  canvas_.begin_frame();
  if (!first_render_done_ || full_refresh_pending_) {
    canvas_.request_full_refresh();
  }
  canvas_.clear(true);
  render_status_bar();

  if (screen_ == Screen::ModeSelector) {
    render_mode_selector();
  } else if (active_mode_ != nullptr) {
    active_mode_->render(canvas_);
  } else {
    render_mode_selector();
  }

  const esp_err_t err = display_.update_canvas(canvas_);
  if (err != ESP_OK) {
    ESP_LOGW(constants::kLogTag, "display update skipped: %s", esp_err_to_name(err));
  } else {
    first_render_done_ = true;
    full_refresh_pending_ = false;
  }
}

void MenuUi::render_status_bar() {
  const char* label = "MENU";
  if (screen_ == Screen::Mode && active_mode_ != nullptr) {
    label = active_mode_->name();
  }
  canvas_.draw_text(2, 2, label, 1, true);

  if (shift_) {
    canvas_.fill_rect(69, 0, 30, 11, true);
    canvas_.draw_text(72, 2, "SHIFT", 1, false);
  } else {
    canvas_.draw_text(72, 2, "SHIFT", 1, true);
  }

  if (alpha_) {
    canvas_.fill_rect(104, 0, 30, 11, true);
    canvas_.draw_text(107, 2, "ALPHA", 1, false);
  } else {
    canvas_.draw_text(107, 2, "ALPHA", 1, true);
  }

  constexpr int kBatteryX = 217;
  constexpr int kBatteryY = 2;
  constexpr int kBatterySlotW = 5;
  for (uint8_t i = 0; i < 5; ++i) {
    const int x = kBatteryX + static_cast<int>(i) * kBatterySlotW;
    canvas_.rect(x, kBatteryY, 4, 8, true);
    if (i < battery_.bars) {
      canvas_.fill_rect(x + 1, kBatteryY + 1, 2, 6, true);
    }
  }
  canvas_.hline(0, 13, MonoCanvas::kWidth, true);
}

void MenuUi::render_mode_selector() {
  canvas_.draw_text(6, 17, "MENU", 1, true);

  for (size_t i = 0; i < constants::kModeCount; ++i) {
    const int y = constants::kModeSelectorY +
                  static_cast<int>(i) * constants::kModeSelectorRowHeight;
    const bool selected = i == selected_mode_;
    if (selected) {
      canvas_.fill_rect(constants::kModeSelectorX - 3,
                        y - 4,
                        constants::kModeSelectorRowWidth,
                        13,
                        true);
    } else {
      canvas_.rect(constants::kModeSelectorX - 3,
                   y - 4,
                   constants::kModeSelectorRowWidth,
                   13,
                   true);
    }

    char label[40] {};
    std::snprintf(label,
                  sizeof(label),
                  "%u %s",
                  static_cast<unsigned>(i),
                  constants::kModeLabels[i]);
    canvas_.draw_text(constants::kModeSelectorX + 4, y, label, 1, !selected);
    canvas_.draw_text(166, y, constants::kModeHints[i], 1, !selected);
  }

  canvas_.draw_text(6, 114, "ARROWS/INDEX/ENTER", 1, true);
}

void MenuUi::apply_apps_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);
  const char* token = key_input(def, key.shift, key.alpha);
  if (app_command(token)) {
    return;
  }

  if (app_stage_ == AppMenuStage::List) {
    const int digit = key_digit(def);
    if (digit >= 0 && static_cast<size_t>(digit) < storage_.app_count()) {
      app_selected_ = static_cast<uint8_t>(digit);
      open_selected_app();
      return;
    }
    switch (def.role) {
      case KeyRole::Left:
      case KeyRole::Up:
        if (storage_.app_count() > 0) {
          app_selected_ = app_selected_ == 0
                              ? static_cast<uint8_t>(storage_.app_count() - 1)
                              : static_cast<uint8_t>(app_selected_ - 1);
        }
        status_ = "PICK APP";
        return;
      case KeyRole::Right:
      case KeyRole::Down:
        if (storage_.app_count() > 0) {
          app_selected_ = static_cast<uint8_t>((app_selected_ + 1) % storage_.app_count());
        }
        status_ = "PICK APP";
        return;
      case KeyRole::Enter:
        open_selected_app();
        return;
      case KeyRole::Clear:
        screen_ = Screen::ModeSelector;
        status_ = "PICK MENU";
        full_refresh_pending_ = true;
        return;
      default:
        return;
    }
  }

  if (app_stage_ == AppMenuStage::Running) {
    if (def.role == KeyRole::Clear) {
      return_to_app_list();
      return;
    }
    if (token != nullptr) {
      app_runtime_.on_key(token);
    }
    return;
  }

  if (def.role == KeyRole::Clear) {
    return_to_app_list();
    return;
  }
}

void MenuUi::render_apps() {
  if (app_stage_ == AppMenuStage::List) {
    canvas_.draw_text(6, 18, "SD / INTERNAL APPS", 1, true);
    if (storage_.app_count() == 0) {
      canvas_.draw_text(8, 45, "No app.ini found", 1, true);
      canvas_.draw_text(8, 59, "/sdcard/programs/<id>", 1, true);
      return;
    }
    for (size_t i = 0; i < storage_.app_count() && i < 6; ++i) {
      const int y = 32 + static_cast<int>(i) * 14;
      const bool selected = i == app_selected_;
      if (selected) {
        canvas_.fill_rect(5, y - 3, 238, 12, true);
      }
      const ExternalAppManifest& app = storage_.app(i);
      char label[64] {};
      std::snprintf(label,
                    sizeof(label),
                    "%u %s %s",
                    static_cast<unsigned>(i),
                    app.name,
                    app.on_sd ? "SD" : "INT");
      canvas_.draw_text(9, y, label, 1, !selected);
    }
    return;
  }

  const char* title = app_runtime_.active_name();
  if (title[0] == '\0' && active_app_index_ < storage_.app_count()) {
    title = storage_.app(active_app_index_).name;
  }
  canvas_.draw_text(6, 18, title, 1, true);
  const bool running = app_runtime_.state() == AppRuntimeState::Running;
  canvas_.draw_text(6, 34, running ? "MPY RUNTIME ACTIVE" : "APP ERROR", 1, true);
  char entry_line[80] {};
  const char* entry_name = std::strrchr(app_runtime_.entry_path(), '/');
  entry_name = entry_name == nullptr ? app_runtime_.entry_path() : entry_name + 1;
  std::snprintf(entry_line, sizeof(entry_line), "entry=%.32s", entry_name);
  canvas_.draw_text(6, 48, entry_line, 1, true);
  char message_line[48] {};
  const char* message = app_runtime_.message();
  if (message[0] == '\0') {
    message = status_;
  }
  std::snprintf(message_line, sizeof(message_line), "%.36s", message);
  canvas_.draw_text(6, 66, message_line, 1, true);
  if (running) {
    char preview_line[48] {};
    std::snprintf(preview_line, sizeof(preview_line), "%.36s", app_runtime_.preview());
    canvas_.draw_text(6, 80, preview_line, 1, true);
  }
  canvas_.draw_text(6, 112, "AC CLOSE", 1, true);
}

}  // namespace esp32calc_alt
