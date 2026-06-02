#include "ui/menu.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "freertos/task.h"
#include "hardware/keymap.h"
#include "ui/modes/mode_common.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "ui";
}  // namespace

const ModeDescriptor& graph_mode_descriptor();

MenuUi::MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display)
    : app_events_(app_events),
      display_(display),
      modes_(builtin_modes()) {}

MenuUi::~MenuUi() {
  close_active_mode();
}

void MenuUi::run() {
  ESP_LOGI(TAG, "starting menu");
  render();

  while (true) {
    AppEvent event {};
    if (xQueueReceive(app_events_, &event, portMAX_DELAY) == pdTRUE) {
      update_state(event);
      while (xQueueReceive(app_events_, &event, 0) == pdTRUE) {
        update_state(event);
      }
      render();
    }
  }
}

void MenuUi::update_state(AppEvent& event) {
  switch (event.type) {
    case AppEventType::Key:
      apply_key(event.key);
      break;
    case AppEventType::Battery:
      battery_ = event.battery;
      break;
    case AppEventType::CalcResult:
      apply_calc_result(event.calc_result);
      event.calc_result = nullptr;
      break;
    default:
      break;
  }
}

void MenuUi::apply_calc_result(CalcResult* result) {
  if (result == nullptr) {
    return;
  }

  if (!result->is_error_ &&
      result->action == CalcResultAction::OpenGraph &&
      result->graph_expression != nullptr) {
    open_mode(graph_mode_descriptor());
    if (active_mode_ != nullptr) {
      apply_mode_result(active_mode_->open_graph_expression(result->graph_expression));
    }
    destroy_calc_result(result);
    return;
  }

  ModeResult mode_result = ModeResult::None;
  if (screen_ == Screen::Mode && active_mode_ != nullptr) {
    mode_result = active_mode_->handle_calc_result(*result);
  }
  destroy_calc_result(result);
  apply_mode_result(mode_result);
}

void MenuUi::apply_mode_result(ModeResult result) {
  switch (result) {
    case ModeResult::ExitToMainMenu:
      close_active_mode();
      screen_ = Screen::Menu;
      full_refresh_pending_ = true;
      return;
    case ModeResult::FullRefresh:
      full_refresh_pending_ = true;
      return;
    case ModeResult::Redraw:
    case ModeResult::None:
      return;
  }
}

void MenuUi::consume_input_modifiers() {
  if (!shift_ && !alpha_) {
    return;
  }
  shift_ = false;
  alpha_ = false;
  ESP_LOGI(TAG, "shift=%d alpha=%d", shift_, alpha_);
}

void MenuUi::apply_key(const KeyEvent& key) {
  if (key.phase != KeyPhase::Pressed) return;

  const KeyDef& def = key_at(key.row, key.col);

  // what do these keys do
  switch (def.role) {
    case KeyRole::Shift: //if pressed "Shift", shift_ boolean is flipped
      shift_ = !shift_;
      ESP_LOGI(TAG, "shift=%d alpha=%d", shift_, alpha_);
      return;
    case KeyRole::Alpha:
      alpha_ = !alpha_;
      ESP_LOGI(TAG, "shift=%d alpha=%d", shift_, alpha_);
      return;
    case KeyRole::Mode:
      if (screen_ != Screen::Menu) {
        close_active_mode();
        screen_ = Screen::Menu;
        full_refresh_pending_ = true;
      }
      return;
    default:
      break;
  }

  // a bit finicky but easy to implement. The main options are treating up/left and right/down the same way, have "weird" interactions around the borders, or have more complicated logic. 
  if (screen_ == Screen::Menu) {
    const int digit = key_digit(def);
    if (digit >= 0 && static_cast<size_t>(digit) < modes_.size()) {
      selected_ = static_cast<uint8_t>(digit);
      open_selected_mode();
      consume_input_modifiers();
      return;
    }

    switch (def.role) {
      case KeyRole::Up:
        move_selection(-2);
        consume_input_modifiers();
        return;
      case KeyRole::Left:
        move_selection(-1);
        consume_input_modifiers();
        return;
      case KeyRole::Down:
        move_selection(2);
        consume_input_modifiers();
        return;
      case KeyRole::Right:
        move_selection(1);
        consume_input_modifiers();
        return;
      case KeyRole::Enter:
        open_selected_mode();
        consume_input_modifiers();
        return;
      default:
        consume_input_modifiers();
        return;
    }

    return;
  }

  if (active_mode_ == nullptr) { // if there is no mode, we need to be in the menu
    screen_ = Screen::Menu;
    full_refresh_pending_ = true;
    return;
  }

  KeyEvent mode_key = key;
  mode_key.shift = shift_;
  mode_key.alpha = alpha_;
  apply_mode_result(active_mode_->handle_key(mode_key, def));
  consume_input_modifiers();
}

void MenuUi::move_selection(int delta) { // here is where the "up/down/left/right" logic would live
  selected_ = move_selection_wrapped(selected_, delta, modes_.size());
}

bool MenuUi::open_mode_by_label(const char* label) {
  if (label == nullptr) {
    return false;
  }

  for (uint8_t i = 0; i < modes_.size(); ++i) {
    if (std::strcmp(modes_[i].label, label) == 0) {
      selected_ = i;
      open_selected_mode();
      return active_mode_ != nullptr;
    }
  }
  return false;
}

void MenuUi::open_mode(const ModeDescriptor& entry) {
  close_active_mode();
  active_mode_ = entry.construct(mode_storage_);
  active_mode_destroy_ = entry.destroy;
  if (active_mode_ != nullptr) {
    active_mode_->on_open();
  }
  screen_ = Screen::Mode;
  full_refresh_pending_ = true;
  ESP_LOGI(TAG, "open mode: %s", entry.label);
}

void MenuUi::open_selected_mode() {
  open_mode(modes_[selected_]);
}

void MenuUi::close_active_mode() { //if mode is not being destroyed and this has been called, destroy so it returns to menu
  if (active_mode_ != nullptr && active_mode_destroy_ != nullptr) {
    active_mode_destroy_(active_mode_);
  }
  active_mode_ = nullptr;
  active_mode_destroy_ = nullptr;
}

void MenuUi::render() {
  canvas_.begin_frame();
  if (!first_render_done_ || full_refresh_pending_) {
    canvas_.request_full_refresh();
  }
  canvas_.clear(true);
  render_status_bar();

  if (screen_ == Screen::Menu) {
    render_menu();
  } else {
    render_content();
  }

  esp_err_t err = display_.update_canvas(canvas_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "display update skipped: %s", esp_err_to_name(err));
  } else {
    first_render_done_ = true;
    full_refresh_pending_ = false;
  }
}

void MenuUi::render_content() {
  if (active_mode_ != nullptr) {
    active_mode_->render(canvas_);
  } else {
    render_menu();
  }
}

void MenuUi::render_status_bar() {
  const char* header_label =
      screen_ == Screen::Mode && active_mode_ != nullptr ? active_mode_->name() : "ESP32CALC";
  canvas_.draw_text(2, 2, header_label, 1, true);

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

  char battery_text[16] {};
  std::snprintf(battery_text, sizeof(battery_text), "%u%%", battery_.percent);
  canvas_.draw_text(224, 2, battery_text, 1, true);
  canvas_.hline(0, 13, MonoCanvas::kWidth, true);
}

void MenuUi::render_menu() {
  canvas_.draw_text(4, 19, "MODE", 2, true);

  for (uint8_t i = 0; i < modes_.size(); ++i) {
    const int x = (i % 2) == 0 ? 5 : 128;
    const int y = 43 + (i / 2) * 18;
    if (i == selected_) {
      canvas_.fill_rect(x - 3, y - 3, 115, 14, true);
      char indexed_label[32] {};
      std::snprintf(indexed_label, sizeof(indexed_label), "%u %s",
                    static_cast<unsigned>(i), modes_[i].label);
      canvas_.draw_text(x, y, indexed_label, 1, false);
    } else {
      canvas_.rect(x - 3, y - 3, 115, 14, true);
      char indexed_label[32] {};
      std::snprintf(indexed_label, sizeof(indexed_label), "%u %s",
                    static_cast<unsigned>(i), modes_[i].label);
      canvas_.draw_text(x, y, indexed_label, 1, true);
    }
  }
}

}  // namespace esp32calc
