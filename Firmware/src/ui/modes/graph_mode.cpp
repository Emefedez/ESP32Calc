#include "ui/modes/mode_registry.h"

#include <cmath>
#include <cstdio>

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"PLOT SIN(X)", "DRAW"},
    {"TABLE", "DATA"},
    {"WINDOW", "AXIS"},
    {"TRACE", "CURSOR"},
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

namespace esp32calc {

class GraphMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  enum class Screen : uint8_t {
    Menu,
    Plot,
    Table,
    Window,
    Trace,
  };

  Screen screen_ = Screen::Menu;
  uint8_t selected_ = 0;

  void open_selected_screen();
  void render_plot(MonoCanvas& canvas);
  void render_table(MonoCanvas& canvas);
  void render_window(MonoCanvas& canvas);
  void render_trace(MonoCanvas& canvas);
};

const ModeDescriptor& graph_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<GraphMode>("GRAPH");
  return kDescriptor;
}

const char* GraphMode::name() const {
  return "GRAPH";
}

void GraphMode::on_open() {
  screen_ = Screen::Menu;
}

ModeResult GraphMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;

  if (screen_ == Screen::Menu) {
    const ModeResult nav = handle_menu_navigation(def, selected_, kItemCount);
    if (nav != ModeResult::None) {
      return nav;
    }

    if (def.role == KeyRole::Enter) {
      open_selected_screen();
      return ModeResult::FullRefresh;
    }
    return ModeResult::None;
  }

  if (def.role == KeyRole::Clear) {
    screen_ = Screen::Menu;
    return ModeResult::FullRefresh;
  }

  return ModeResult::None;
}

void GraphMode::render(MonoCanvas& canvas) {
  switch (screen_) {
    case Screen::Menu:
      render_mode_menu(canvas, name(), kItems, kItemCount, selected_);
      return;
    case Screen::Plot:
      render_plot(canvas);
      return;
    case Screen::Table:
      render_table(canvas);
      return;
    case Screen::Window:
      render_window(canvas);
      return;
    case Screen::Trace:
      render_trace(canvas);
      return;
  }
}

void GraphMode::open_selected_screen() {
  switch (selected_) {
    case 0:
      screen_ = Screen::Plot;
      return;
    case 1:
      screen_ = Screen::Table;
      return;
    case 2:
      screen_ = Screen::Window;
      return;
    default:
      screen_ = Screen::Trace;
      return;
  }
}

void GraphMode::render_plot(MonoCanvas& canvas) {
  canvas.draw_text(4, 19, "GRAPH", 2, true);

  constexpr int gx = 8;
  constexpr int gy = 43;
  constexpr int gw = MonoCanvas::kWidth - gx * 2;
  constexpr int gh = 75;
  constexpr int x_axis = gy + gh / 2;
  constexpr int y_axis = gx + 24;

  canvas.rect(gx, gy, gw, gh, true);
  canvas.hline(gx, x_axis, gw, true);
  canvas.vline(y_axis, gy, gh, true);

  constexpr int kGraphPoints = 96;
  int prev_x = gx + 1;
  float prev_fx = static_cast<float>(prev_x - y_axis) / 25.0f;
  int prev_y = x_axis - static_cast<int>(30.0f * std::sin(prev_fx));
  for (int i = 1; i < kGraphPoints; ++i) {
    const int x = gx + 1 + i * (gw - 3) / (kGraphPoints - 1);
    const float fx = static_cast<float>(x - y_axis) / 25.0f;
    const int y = x_axis - static_cast<int>(30.0f * std::sin(fx));
    canvas.DrawLine(prev_x, prev_y, x, y, true);
    prev_x = x;
    prev_y = y;
  }

  canvas.draw_text(190, 31, "SIN(X)", 1, true);
}

void GraphMode::render_table(MonoCanvas& canvas) {
  render_mode_title(canvas, name());
  canvas.draw_text(8, 42, "X", 1, true);
  canvas.draw_text(78, 42, "SIN(X)", 1, true);
  canvas.hline(6, 53, 180, true);

  constexpr float kValues[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
  for (size_t i = 0; i < sizeof(kValues) / sizeof(kValues[0]); ++i) {
    char x_text[16] {};
    char y_text[16] {};
    std::snprintf(x_text, sizeof(x_text), "%.1f", kValues[i]);
    std::snprintf(y_text, sizeof(y_text), "%.2f", std::sin(kValues[i]));

    const int y = 62 + static_cast<int>(i) * 11;
    canvas.draw_text(8, y, x_text, 1, true);
    canvas.draw_text(78, y, y_text, 1, true);
  }
}

void GraphMode::render_window(MonoCanvas& canvas) {
  render_mode_title(canvas, name());
  canvas.draw_text(8, 43, "WINDOW", 1, true);
  canvas.hline(8, 55, 102, true);
  canvas.draw_text(8, 66, "XMIN -5", 1, true);
  canvas.draw_text(8, 78, "XMAX  5", 1, true);
  canvas.draw_text(8, 90, "YMIN -1", 1, true);
  canvas.draw_text(8, 102, "YMAX  1", 1, true);
}

void GraphMode::render_trace(MonoCanvas& canvas) {
  render_mode_title(canvas, name());
  canvas.draw_text(8, 43, "TRACE", 1, true);
  canvas.hline(8, 55, 78, true);
  canvas.draw_text(8, 68, "X = 0.00", 1, true);
  canvas.draw_text(8, 82, "Y = 0.00", 1, true);
  canvas.draw_text(8, 100, "CLR RETURNS GRAPH MENU", 1, true);
}

}  // namespace esp32calc
