#include "ui/menu.h"

#include <cstddef>
#include <cstdio>
#include <new>

#include "esp_log.h"
#include "freertos/task.h"
#include "hardware/keymap.h"
#include "ui/menu_constants.h"

namespace esp32calc_alt {
namespace {

namespace constants = menu_constants;

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

class GraphMenuMode final : public MenuMode {
 public:
  explicit GraphMenuMode(MenuUi& owner) : owner_(owner) {}
  const char* name() const override { return "GRAPH"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_graph_key(key); }
  void handle_math_result(const MathResult& result) override { owner_.apply_graph_result(result); }
  void render(MonoCanvas&) override { owner_.render_graph(); }

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
  const char* name() const override { return "INTEGRALS"; }
  void handle_key(const KeyEvent& key) override { owner_.apply_integrals_key(key); }
  void render(MonoCanvas&) override { owner_.render_integrals(); }

 private:
  MenuUi& owner_;
};

MenuUi::MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display, MathService& math)
    : app_events_(app_events), display_(display), math_(math) {
  open_mode(ModeKind::Standard);
}

MenuUi::~MenuUi() {
  close_active_mode();
}

void MenuUi::run() {
  ESP_LOGI(constants::kLogTag, "starting alt menu");
  render();

  while (true) {
    AppEvent event {};
    if (xQueueReceive(app_events_, &event, pdMS_TO_TICKS(25)) == pdTRUE) {
      if (event.type == AppEventType::Key) {
        update_from_key(event.key);
      }
      while (xQueueReceive(app_events_, &event, 0) == pdTRUE) {
        if (event.type == AppEventType::Key) {
          update_from_key(event.key);
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
  }
}

void MenuUi::update_from_key(const KeyEvent& key) {
  if (key.phase != KeyPhase::Pressed) {
    return;
  }

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
      return ModeKind::Graph;
    case 2:
      return ModeKind::Constants;
    case 3:
      return ModeKind::Integrals;
    case 0:
    default:
      return ModeKind::Standard;
  }
}

uint8_t MenuUi::index_from_mode(ModeKind kind) const {
  switch (kind) {
    case ModeKind::Graph:
      return 1;
    case ModeKind::Constants:
      return 2;
    case ModeKind::Integrals:
      return 3;
    case ModeKind::Standard:
    default:
      return 0;
  }
}

void MenuUi::open_mode(ModeKind kind) {
  close_active_mode();
  active_mode_kind_ = kind;
  selected_mode_ = index_from_mode(kind);

  switch (kind) {
    case ModeKind::Integrals:
      static_assert(sizeof(IntegralsMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) IntegralsMenuMode(*this);
      integral_stage_ = IntegralMenuStage::Groups;
      clear_integral_search();
      status_ = "PICK GROUP";
      break;
    case ModeKind::Constants:
      static_assert(sizeof(ConstantsMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) ConstantsMenuMode(*this);
      constant_stage_ = ConstantMenuStage::Groups;
      clear_constant_search();
      status_ = "PICK GROUP";
      break;
    case ModeKind::Graph:
      static_assert(sizeof(GraphMenuMode) <= constants::kModeStorageSize);
      active_mode_ = new (mode_storage_) GraphMenuMode(*this);
      status_ = "GRAPH MODE";
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
      case ModeKind::Constants:
        static_cast<ConstantsMenuMode*>(active_mode_)->~ConstantsMenuMode();
        break;
      case ModeKind::Graph:
        static_cast<GraphMenuMode*>(active_mode_)->~GraphMenuMode();
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

  canvas_.draw_text(209, 2, "ALT", 1, true);
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

  canvas_.draw_text(6, 108, "ARROWS/INDEX/ENTER", 1, true);
}

}  // namespace esp32calc_alt
