#include "ui/menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "esp_log.h"
#include "freertos/task.h"
#include "keymap.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "ui";

constexpr std::array<const char*, 7> kModes = {
    "STANDARD", "GRAPH", "SYMBOLIC",
    "PROGRAMS", "FILES", "WIRELESS", "SETTINGS",
};

}  // namespace

MenuUi::MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display)
    : app_events_(app_events), display_(display) {}

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

  switch (def.role) {
    case KeyRole::Shift:
      shift_ = !shift_;
      ESP_LOGI(TAG, "shift=%d alpha=%d", shift_, alpha_);
      return;
    case KeyRole::Alpha:
      alpha_ = !alpha_;
      ESP_LOGI(TAG, "shift=%d alpha=%d", shift_, alpha_);
      return;
    case KeyRole::Mode:
      if (screen_ != Screen::Menu) {
        screen_ = Screen::Menu;
        full_refresh_pending_ = true;
      }
      return;
    default:
      break;
  }

  if (screen_ == Screen::Menu) {
    switch (def.role) {
      case KeyRole::Up:
      case KeyRole::Left:
        move_selection(-1);
        return;
      case KeyRole::Down:
      case KeyRole::Right:
        move_selection(1);
        return;
      case KeyRole::Enter:
        open_selected_mode();
        return;
      default:
        return;
    }
  } else if (def.role == KeyRole::Clear) {
    screen_ = Screen::Menu;
    full_refresh_pending_ = true;
  }
}

void MenuUi::move_selection(int delta) {
  const int count = static_cast<int>(kModes.size());
  int next = static_cast<int>(selected_) + delta;
  if (next < 0) next = count - 1;
  else if (next >= count) next = 0;
  selected_ = static_cast<uint8_t>(next);
}

void MenuUi::open_selected_mode() {
  screen_ = screen_for_index(selected_);
  full_refresh_pending_ = true;
  ESP_LOGI(TAG, "open mode: %s", kModes[selected_]);
}

MenuUi::Screen MenuUi::screen_for_index(uint8_t index) const {
  switch (index) {
    case 0: return Screen::Standard;
    case 1: return Screen::Graph;
    case 2: return Screen::Symbolic;
    case 3: return Screen::Programs;
    case 4: return Screen::Files;
    default: return Screen::Settings;
  }
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
  switch (screen_) {
    case Screen::Graph:
      render_graph();
      return;
    case Screen::Standard:
    case Screen::Symbolic:
    case Screen::Programs:
    case Screen::Files:
    case Screen::Wireless:
    case Screen::Settings:
      render_placeholder("MODE", "SELECT FROM MENU");
      return;
    case Screen::Menu:
      return;
  }
}

void MenuUi::render_graph() {
  canvas_.draw_text(4, 19, "GRAPH", 2, true);

  constexpr int gx = 8;
  constexpr int gy = 43;
  constexpr int gw = MonoCanvas::kWidth - gx * 2;
  constexpr int gh = 75;
  constexpr int x_axis = gy + gh / 2;
  constexpr int y_axis = gx + 24;

  canvas_.rect(gx, gy, gw, gh, true);
  canvas_.hline(gx, x_axis, gw, true);
  canvas_.vline(y_axis, gy, gh, true);

  constexpr int kGraphPoints = 96;
  int prev_x = gx + 1;
  float prev_fx = static_cast<float>(prev_x - y_axis) / 25.0f;
  int prev_y = x_axis - static_cast<int>(30.0f * std::sin(prev_fx));
  for (int i = 1; i < kGraphPoints; ++i) {
    const int x = gx + 1 + i * (gw - 3) / (kGraphPoints - 1);
    const float fx = static_cast<float>(x - y_axis) / 25.0f;
    const int y = x_axis - static_cast<int>(30.0f * std::sin(fx));
    canvas_.DrawLine(prev_x, prev_y, x, y, true);
    prev_x = x;
    prev_y = y;
  }
  canvas_.draw_text(190, 31, "SIN(X)", 1, true);
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

  for (uint8_t i = 0; i < kModes.size(); ++i) {
    const int x = (i % 2) == 0 ? 5 : 128;
    const int y = 43 + (i / 2) * 18;
    if (i == selected_) {
      canvas_.fill_rect(x - 3, y - 3, 115, 14, true);
      canvas_.draw_text(x, y, kModes[i], 1, false);
    } else {
      canvas_.rect(x - 3, y - 3, 115, 14, true);
      canvas_.draw_text(x, y, kModes[i], 1, true);
    }
  }
}

void MenuUi::render_placeholder(const char* title, const char* subtitle) {
  canvas_.draw_text(4, 22, title, 2, true);
  canvas_.draw_text(5, 49, subtitle, 1, true);
  canvas_.hline(5, 64, 240, true);
  canvas_.draw_text(5, 76, "MODE RETURNS MENU", 1, true);
}

}  // namespace esp32calc
