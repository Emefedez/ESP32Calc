#include "ui/menu.h"

#include <algorithm>
#include <cstdio>

#include "app_log.h"
#include "freertos/task.h"
#include "keymap.h"
#if ESP32CALC_USE_RAYLIB
#include "raylib.h"
#endif

namespace esp32calc {
namespace {
constexpr const char* TAG = "ui";

constexpr std::array<const char*, 7> kModes = {
    "STANDARD",
    "GRAPH",
    "SYMBOLIC",
    "PROGRAMS",
    "FILES",
    "WIRELESS",
    "SETTINGS",
};
}  // namespace

MenuUi::MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display)
    : app_events_(app_events), display_(display) {}

void MenuUi::run() {
  ESP32CALC_LOGI(TAG, "starting menu");
  render(RefreshMode::Full);

  while (true) {
    AppEvent event {};
    if (xQueueReceive(app_events_, &event, portMAX_DELAY) == pdTRUE) {
      handle_event(event);
    }
  }
}

void MenuUi::handle_event(const AppEvent& event) {
  switch (event.type) {
    case AppEventType::Key:
      handle_key(event.key);
      break;
    case AppEventType::Battery:
      battery_ = event.battery;
      render(RefreshMode::Fast);
      break;
    default:
      break;
  }
}

void MenuUi::handle_key(const KeyEvent& key) {
  if (key.phase != KeyPhase::Pressed) {
    return;
  }

  const KeyDef& def = key_at(key.row, key.col);

  if (def.role == KeyRole::Shift) {
    shift_ = !shift_;
    ESP32CALC_LOGI(TAG, "shift=%d alpha=%d", shift_, alpha_);
    render(RefreshMode::Fast);
    return;
  }

  if (def.role == KeyRole::Alpha) {
    alpha_ = !alpha_;
    ESP32CALC_LOGI(TAG, "shift=%d alpha=%d", shift_, alpha_);
    render(RefreshMode::Fast);
    return;
  }

  if (def.role == KeyRole::Mode) {
    screen_ = Screen::Menu;
    render(RefreshMode::Fast);
    return;
  }

  if (screen_ == Screen::Menu) {
    switch (def.role) {
      case KeyRole::Up:
      case KeyRole::Left:
        move_selection(-1);
        render(RefreshMode::Fast);
        break;
      case KeyRole::Down:
      case KeyRole::Right:
        move_selection(1);
        render(RefreshMode::Fast);
        break;
      case KeyRole::Enter:
        open_selected_mode();
        render(RefreshMode::Fast);
        break;
      default:
        break;
    }
  } else if (def.role == KeyRole::Clear) {
    screen_ = Screen::Menu;
    render(RefreshMode::Fast);
  }
}

void MenuUi::move_selection(int delta) {
  const int count = static_cast<int>(kModes.size());
  int next = static_cast<int>(selected_) + delta;
  if (next < 0) {
    next = count - 1;
  } else if (next >= count) {
    next = 0;
  }
  selected_ = static_cast<uint8_t>(next);
}

void MenuUi::open_selected_mode() {
  screen_ = screen_for_index(selected_);
  ESP32CALC_LOGI(TAG, "open mode: %s", kModes[selected_]);
}

MenuUi::Screen MenuUi::screen_for_index(uint8_t index) const {
  switch (index) {
    case 0: return Screen::Standard;
    case 1: return Screen::Graph;
    case 2: return Screen::Symbolic;
    case 3: return Screen::Programs;
    case 4: return Screen::Files;
    case 5: return Screen::Wireless;
    default: return Screen::Settings;
  }
}

void MenuUi::render(RefreshMode mode) {
#if ESP32CALC_USE_RAYLIB
  render_raylib(mode);
  return;
#else
  canvas_.clear(true);
  render_status_bar();

  if (screen_ == Screen::Menu) {
    render_menu();
  } else {
    switch (screen_) {
      case Screen::Standard:
        render_placeholder("STANDARD", "NUMERIC ENTRY");
        break;
      case Screen::Graph:
        render_placeholder("GRAPH", "PLOT PIPELINE");
        break;
      case Screen::Symbolic:
        render_placeholder("SYMBOLIC", "CAS ENGINE");
        break;
      case Screen::Programs:
        render_placeholder("PROGRAMS", "/SDCARD/PROGRAMS");
        break;
      case Screen::Files:
        render_placeholder("FILES", "SD BROWSER");
        break;
      case Screen::Wireless:
        render_placeholder("WIRELESS", "RESERVED");
        break;
      case Screen::Settings:
        render_placeholder("SETTINGS", "DEVICE CONFIG");
        break;
      case Screen::Menu:
        break;
    }
  }

  RefreshMode actual_mode = RefreshMode::Full;
#if !ESP32CALC_FORCE_FULL_REFRESH
  actual_mode = first_render_done_ ? mode : RefreshMode::Full;
#else
  (void)mode;
#endif
  esp_err_t err = display_.update_canvas(canvas_, actual_mode);
  if (err != ESP_OK) {
    ESP32CALC_LOGW(TAG, "display update skipped: %s", esp_err_to_name(err));
  } else {
    first_render_done_ = true;
  }
#endif
}

#if ESP32CALC_USE_RAYLIB
void MenuUi::render_raylib(RefreshMode mode) {
  esp_err_t err = raylib_.init(display_);
  if (err != ESP_OK) {
    ESP32CALC_LOGW(TAG, "raylib init skipped: %s", esp_err_to_name(err));
    return;
  }

  RefreshMode actual = RefreshMode::Full;
#if !ESP32CALC_FORCE_FULL_REFRESH
  actual = first_render_done_ ? mode : RefreshMode::Full;
#else
  (void)mode;
#endif
  raylib_.set_next_refresh_mode(actual);

  BeginDrawing();
  ClearBackground(RAYWHITE);
  render_status_bar_raylib();

  if (screen_ == Screen::Menu) {
    render_menu_raylib();
  } else {
    switch (screen_) {
      case Screen::Standard:
        render_placeholder_raylib("STANDARD", "NUMERIC ENTRY");
        break;
      case Screen::Graph:
        render_placeholder_raylib("GRAPH", "PLOT PIPELINE");
        break;
      case Screen::Symbolic:
        render_placeholder_raylib("SYMBOLIC", "CAS ENGINE");
        break;
      case Screen::Programs:
        render_placeholder_raylib("PROGRAMS", "/SDCARD/PROGRAMS");
        break;
      case Screen::Files:
        render_placeholder_raylib("FILES", "SD BROWSER");
        break;
      case Screen::Wireless:
        render_placeholder_raylib("WIRELESS", "RESERVED");
        break;
      case Screen::Settings:
        render_placeholder_raylib("SETTINGS", "DEVICE CONFIG");
        break;
      case Screen::Menu:
        break;
    }
  }

  EndDrawing();
  first_render_done_ = true;
}

void MenuUi::render_status_bar_raylib() {
  DrawText("ESP32CALC", 2, 2, 8, BLACK);

  if (shift_) {
    DrawRectangle(68, 0, 32, 12, BLACK);
    DrawText("SHIFT", 72, 2, 8, WHITE);
  } else {
    DrawText("SHIFT", 72, 2, 8, BLACK);
  }

  if (alpha_) {
    DrawRectangle(103, 0, 33, 12, BLACK);
    DrawText("ALPHA", 107, 2, 8, WHITE);
  } else {
    DrawText("ALPHA", 107, 2, 8, BLACK);
  }

  char battery_text[16] {};
  std::snprintf(battery_text, sizeof(battery_text), "%u%%", battery_.percent);
  DrawText(battery_text, 224, 2, 8, BLACK);
  DrawLine(0, 13, MonoCanvas::kWidth, 13, BLACK);
}

void MenuUi::render_menu_raylib() {
  DrawText("MODE", 4, 19, 16, BLACK);

  for (uint8_t i = 0; i < kModes.size(); ++i) {
    const int x = (i % 2) == 0 ? 5 : 128;
    const int y = 43 + (i / 2) * 18;
    if (i == selected_) {
      DrawRectangle(x - 3, y - 3, 115, 14, BLACK);
      DrawText(kModes[i], x, y, 8, WHITE);
    } else {
      DrawRectangleLines(x - 3, y - 3, 115, 14, BLACK);
      DrawText(kModes[i], x, y, 8, BLACK);
    }
  }
}

void MenuUi::render_placeholder_raylib(const char* title, const char* subtitle) {
  DrawText(title, 4, 22, 16, BLACK);
  DrawText(subtitle, 5, 49, 8, BLACK);
  DrawLine(5, 64, 245, 64, BLACK);
  DrawText("MODE RETURNS MENU", 5, 76, 8, BLACK);
}
#endif

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
