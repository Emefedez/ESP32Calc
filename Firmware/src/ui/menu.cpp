#include "ui/menu.h"

#include <cstdio>

#include "esp_log.h"
#include "freertos/task.h"
#include "hardware/keymap.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "ui";
}  // namespace

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

void MenuUi::update_state(const AppEvent& event) {
  switch (event.type) {
    case AppEventType::Key:
      apply_key(event.key);
      break;
    case AppEventType::Battery:
      battery_ = event.battery;
      break;
    default:
      break;
  }
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
    switch (def.role) {
      case KeyRole::Up:
        move_selection(-2);
        return;
      case KeyRole::Left:
        move_selection(-1);
        return;
      case KeyRole::Down:
        move_selection(2);
        return;
      case KeyRole::Right:
        move_selection(1);
        return;
      case KeyRole::Enter:
        open_selected_mode();
        return;
      default:
        return;
    }

    return;
  }

  if (active_mode_ == nullptr) { // if there is no mode, we need to be in the menu
    screen_ = Screen::Menu;
    full_refresh_pending_ = true;
    return;
  }

  const ModeResult result = active_mode_->handle_key(key, def);
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

void MenuUi::move_selection(int delta) { // here is where the "up/down/left/right" logic would live
  const int count = static_cast<int>(modes_.size()); // we take the amount of modes
  int next = static_cast<int>(selected_) + delta; // we pass what the key press tells us
  //rollover checks
  if (next < 0) next = count - 1; 
  else if (next >= count) next = 0;
  selected_ = static_cast<uint8_t>(next);
}

void MenuUi::open_selected_mode() {
  close_active_mode();
  const ModeDescriptor& entry = modes_[selected_];
  active_mode_ = entry.construct(mode_storage_);
  active_mode_destroy_ = entry.destroy;
  if (active_mode_ != nullptr) {
    active_mode_->on_open();
  }
  screen_ = Screen::Mode;
  full_refresh_pending_ = true;
  ESP_LOGI(TAG, "open mode: %s", modes_[selected_].label);
}

void MenuUi::close_active_mode() {
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
  canvas_.draw_text(2, 2, "ESP32CALC", 1, true);

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
      canvas_.draw_text(x, y, modes_[i].label, 1, false);
    } else {
      canvas_.rect(x - 3, y - 3, 115, 14, true);
      canvas_.draw_text(x, y, modes_[i].label, 1, true);
    }
  }
}

}  // namespace esp32calc
