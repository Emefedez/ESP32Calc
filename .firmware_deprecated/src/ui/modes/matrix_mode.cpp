#include "ui/modes/mode_registry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

#include "calc/calc_engine.h"
#include "ui/modes/mode_common.h"

extern esp32calc::CalcEngine g_calc;

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"SOLVE", "AX=B"},
    {"EDIT", "MAT"},
    {"DET", "|A|"},
    {"INVERSE", "A^-1"},
};
constexpr const char* kMessages[] = {
    "MATRIX SOLVER",
    "MATRIX EDITOR",
    "DETERMINANT",
    "INVERSE MATRIX",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);
constexpr size_t kSystemCapacity = 128;
constexpr size_t kVisibleSystemChars = 38;
constexpr size_t kVisibleResultChars = 36;
constexpr int kCharWidth = 6;
constexpr int kInputTextX = 10;
constexpr int kInputTextY = 42;

char* duplicate_text(const char* text) {
  if (text == nullptr) {
    return nullptr;
  }

  const size_t len = std::strlen(text) + 1;
  char* copy = new (std::nothrow) char[len];
  if (copy == nullptr) {
    return nullptr;
  }
  std::memcpy(copy, text, len);
  return copy;
}

}  // namespace

namespace esp32calc {

class MatrixMode final : public UiMode {
 public:
  ~MatrixMode() override;

  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  ModeResult handle_calc_result(const CalcResult& result) override;
  void render(MonoCanvas& canvas) override;

 private:
  SimpleMenuState menu_ {};
  char system_[kSystemCapacity] {};
  size_t system_len_ = 0;
  size_t cursor_ = 0;
  const char* status_ = "ENTER SOLVES";
  char* result_text_ = nullptr;
  bool result_is_error_ = false;

  bool append_system_text(const char* token);
  void delete_system_char();
  void clear_system();
  void clear_result();
  void move_cursor_left(bool all_the_way);
  void move_cursor_right(bool all_the_way);
  size_t system_visible_start() const;
  bool set_result_text(const char* text);
  ModeResult handle_solver_key(const KeyEvent& key, const KeyDef& def);
  void render_solver(MonoCanvas& canvas);
};

const ModeDescriptor& matrix_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<MatrixMode>("MATRIX");
  return kDescriptor;
}

const char* MatrixMode::name() const {
  return "MATRIX";
}

MatrixMode::~MatrixMode() {
  clear_result();
}

void MatrixMode::on_open() {
  menu_ = {};
  clear_system();
  clear_result();
  status_ = "ENTER SOLVES";
}

ModeResult MatrixMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  if (menu_.detail_open && (menu_.selected == 0 || menu_.selected == 2 || menu_.selected == 3)) {
    return handle_solver_key(key, def);
  }

  (void)key;
  return handle_simple_menu_key(def, menu_, kItemCount);
}

ModeResult MatrixMode::handle_calc_result(const CalcResult& result) {
  if (!menu_.detail_open || menu_.selected != 0 || result.action != CalcResultAction::ShowText) {
    return ModeResult::None;
  }

  const char* text = result.display_text;
  if (text == nullptr || text[0] == '\0') {
    text = result.is_error_ ? "ERROR" : "OK";
  }

  if (!set_result_text(text)) {
    result_is_error_ = true;
    status_ = "NO MEMORY";
    return ModeResult::Redraw;
  }

  result_is_error_ = result.is_error_;
  status_ = result.is_error_ ? "ERROR" : "DONE";
  return ModeResult::Redraw;
}

void MatrixMode::render(MonoCanvas& canvas) {
  if (menu_.detail_open && menu_.selected == 0) {
    render_solver(canvas);
    return;
  }

  render_simple_menu(canvas, name(), kItems, kMessages, kItemCount, menu_);
}

bool MatrixMode::append_system_text(const char* token) {
  const size_t token_len = std::strlen(token);
  if (token_len == 0 || system_len_ + token_len >= sizeof(system_)) {
    return false;
  }

  if (cursor_ > system_len_) {
    cursor_ = system_len_;
  }

  std::memmove(system_ + cursor_ + token_len, system_ + cursor_, system_len_ - cursor_ + 1);
  std::memcpy(system_ + cursor_, token, token_len);
  system_len_ += token_len;
  cursor_ += token_len;
  system_[system_len_] = '\0';
  clear_result();
  return true;
}

void MatrixMode::delete_system_char() {
  if (system_len_ == 0 || cursor_ == 0) {
    return;
  }

  std::memmove(system_ + cursor_ - 1, system_ + cursor_, system_len_ - cursor_ + 1);
  --system_len_;
  --cursor_;
  system_[system_len_] = '\0';
  clear_result();
}

void MatrixMode::clear_system() {
  system_len_ = 0;
  cursor_ = 0;
  system_[0] = '\0';
  clear_result();
}

void MatrixMode::clear_result() {
  delete[] result_text_;
  result_text_ = nullptr;
  result_is_error_ = false;
}

void MatrixMode::move_cursor_left(bool all_the_way) {
  if (all_the_way) {
    cursor_ = 0;
  } else if (cursor_ > 0) {
    --cursor_;
  }
}

void MatrixMode::move_cursor_right(bool all_the_way) {
  if (all_the_way) {
    cursor_ = system_len_;
  } else if (cursor_ < system_len_) {
    ++cursor_;
  }
}

size_t MatrixMode::system_visible_start() const {
  if (system_len_ <= kVisibleSystemChars) {
    return 0;
  }

  const size_t half_window = kVisibleSystemChars / 2;
  if (cursor_ <= half_window) {
    return 0;
  }

  const size_t max_start = system_len_ - kVisibleSystemChars;
  return std::min(cursor_ - half_window, max_start);
}

bool MatrixMode::set_result_text(const char* text) {
  char* copy = duplicate_text(text);
  if (copy == nullptr) {
    return false;
  }

  clear_result();
  result_text_ = copy;
  return true;
}

ModeResult MatrixMode::handle_solver_key(const KeyEvent& key, const KeyDef& def) {
  if (def.role == KeyRole::Clear) {
    if (system_len_ == 0 && result_text_ == nullptr) {
      menu_.detail_open = false;
      return ModeResult::FullRefresh;
    }

    clear_system();
    status_ = "ENTER SOLVES";
    return ModeResult::Redraw;
  }

  if (def.role == KeyRole::Delete) {
    delete_system_char();
    status_ = "ENTER SOLVES";
    return ModeResult::Redraw;
  }

  if (def.role == KeyRole::Left) {
    move_cursor_left(key.shift);
    status_ = "EDIT";
    return ModeResult::Redraw;
  }

  if (def.role == KeyRole::Right) {
    move_cursor_right(key.shift);
    status_ = "EDIT";
    return ModeResult::Redraw;
  }

  if (def.role == KeyRole::Up) {
    cursor_ = 0;
    status_ = "EDIT";
    return ModeResult::Redraw;
  }

  if (def.role == KeyRole::Down) {
    cursor_ = system_len_;
    status_ = "EDIT";
    return ModeResult::Redraw;
  }

  if (def.role == KeyRole::Enter) {
    if (std::strcmp(def.label, "=") == 0 && key.shift) {
      status_ = append_system_text("=") ? "ENTER SOLVES" : "EXPR FULL";
      return ModeResult::Redraw;
    }

    if (std::strcmp(def.label, "=") == 0 && key.alpha) {
      status_ = append_system_text(";") ? "NEXT EQUATION" : "EXPR FULL";
      return ModeResult::Redraw;
    }

    if (system_len_ == 0) {
      status_ = "EMPTY SYSTEM";
      return ModeResult::Redraw;
    }

    bool submitted = false;
    if (menu_.selected == 0) {
      submitted = g_calc.submit_solve_expression(system_);
    } else {
      char request[160] {};
      std::snprintf(request,
                    sizeof(request),
                    "%s(%s)",
                    menu_.selected == 2 ? "det" : "inv",
                    system_);
      submitted = g_calc.submit_expression(request);
    }
    status_ = submitted ? "SOLVE QUEUED" : "QUEUE FULL";
    if (submitted) {
      clear_result();
    }
    return ModeResult::Redraw;
  }

  const char* token = key_input(def, key.shift, key.alpha);
  if (token != nullptr && append_system_text(token)) {
    status_ = "ENTER SOLVES";
    return ModeResult::Redraw;
  }

  return ModeResult::None;
}

void MatrixMode::render_solver(MonoCanvas& canvas) {
  const char* title = menu_.selected == 0 ? "LINEAR SYSTEM"
                                          : (menu_.selected == 2 ? "DET MATRIX"
                                                                 : "INVERSE MATRIX");
  canvas.draw_text(6, 17, title, 1, true);
  canvas.rect(5, 30, 240, 28, true);

  const size_t visible_start = system_visible_start();
  const size_t visible_count =
      system_len_ > visible_start ? std::min(kVisibleSystemChars, system_len_ - visible_start) : 0;
  char visible_system[kVisibleSystemChars + 1] {};
  if (visible_count > 0) {
    std::memcpy(visible_system, system_ + visible_start, visible_count);
    visible_system[visible_count] = '\0';
  }

  canvas.draw_text(kInputTextX, kInputTextY, system_len_ == 0 ? "0" : visible_system, 1, true);

  const size_t cursor_relative = cursor_ > visible_start ? cursor_ - visible_start : 0;
  const size_t cursor_cells = system_len_ == 0
                                  ? 1
                                  : std::min(cursor_relative, kVisibleSystemChars);
  const int cursor_x = kInputTextX + static_cast<int>(cursor_cells) * kCharWidth;
  canvas.vline(cursor_x, kInputTextY - 3, 13, true);

  if (result_text_ != nullptr) {
    const size_t result_len = std::strlen(result_text_);
    const size_t max_visible_result = kVisibleResultChars * 2;
    size_t result_offset = 0;
    if (result_len > max_visible_result) {
      result_offset = result_len - max_visible_result;
    }

    canvas.draw_text(6, 65, result_is_error_ ? "ERR" : "=", 1, true);
    for (size_t line = 0; line < 2; ++line) {
      const size_t line_offset = result_offset + line * kVisibleResultChars;
      if (line_offset >= result_len) {
        break;
      }

      const size_t line_len = std::min(kVisibleResultChars, result_len - line_offset);
      char line_text[kVisibleResultChars + 1] {};
      std::memcpy(line_text, result_text_ + line_offset, line_len);
      line_text[line_len] = '\0';
      canvas.draw_text(28, 65 + static_cast<int>(line) * 12, line_text, 1, true);
    }
  }

  canvas.hline(5, 98, 240, true);
  canvas.draw_text(6, 108, status_, 1, true);
  canvas.draw_text(96, 108, "A/=,  A==;  S==", 1, true);
}

}  // namespace esp32calc
