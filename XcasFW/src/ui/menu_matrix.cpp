#include "ui/menu.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "graphics/mono_canvas.h"
#include "ui/menu_constants.h"

namespace esp32calc_alt {
namespace {

namespace constants = menu_constants;

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

bool append_text(char* output, size_t output_size, const char* text) {
  if (!has_text(text)) {
    return true;
  }
  const size_t used = std::strlen(output);
  const size_t add = std::strlen(text);
  if (used + add >= output_size) {
    return false;
  }
  std::memcpy(output + used, text, add + 1);
  return true;
}

void visible_cell_text(const char* source, char* output, size_t output_size, size_t max_chars) {
  const char* text = has_text(source) ? source : "0";
  const size_t length = std::strlen(text);
  const size_t begin = length > max_chars ? length - max_chars : 0;
  std::snprintf(output, output_size, "%s", text + begin);
}

}  // namespace

void MenuUi::apply_matrix_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);
  const int digit = key_digit(def);

  if (def.role == KeyRole::Mode) {
    screen_ = Screen::ModeSelector;
    full_refresh_pending_ = true;
    return;
  }

  if (matrix_stage_ == MatrixMenuStage::Matrices) {
    if (digit >= 0 && static_cast<size_t>(digit) < constants::kMatrixCount) {
      matrix_selected_ = static_cast<uint8_t>(digit);
      matrix_stage_ = MatrixMenuStage::Size;
      status_ = "PICK SIZE";
      return;
    }

    switch (def.role) {
      case KeyRole::Left:
        move_matrix_selection(-1);
        status_ = "PICK MATRIX";
        return;
      case KeyRole::Right:
        move_matrix_selection(1);
        status_ = "PICK MATRIX";
        return;
      case KeyRole::Enter:
        if (key_is_equals(key)) {
          insert_matrix_name();
        } else {
          matrix_stage_ = MatrixMenuStage::Size;
          status_ = "PICK SIZE";
        }
        return;
      case KeyRole::Clear:
        open_mode(ModeKind::Standard);
        return;
      default:
        return;
    }
  }

  if (matrix_stage_ == MatrixMenuStage::Size) {
    if (digit >= 1 && digit <= static_cast<int>(constants::kMatrixMaxRows) &&
        static_cast<size_t>(digit) <= constants::kMatrixMaxCols) {
      matrix_rows_[matrix_selected_] = static_cast<uint8_t>(digit);
      matrix_cols_[matrix_selected_] = static_cast<uint8_t>(digit);
      matrix_cell_row_ = 0;
      matrix_cell_col_ = 0;
      status_ = "SQUARE SIZE";
      return;
    }

    switch (def.role) {
      case KeyRole::Left:
        adjust_matrix_cols(-1);
        status_ = "PICK SIZE";
        return;
      case KeyRole::Right:
        adjust_matrix_cols(1);
        status_ = "PICK SIZE";
        return;
      case KeyRole::Up:
        adjust_matrix_rows(1);
        status_ = "PICK SIZE";
        return;
      case KeyRole::Down:
        adjust_matrix_rows(-1);
        status_ = "PICK SIZE";
        return;
      case KeyRole::FractionToggle:
        fill_matrix_identity(matrix_selected_);
        matrix_stage_ = MatrixMenuStage::Cells;
        status_ = "IDENTITY";
        return;
      case KeyRole::Delete:
        fill_matrix_zero(matrix_selected_);
        status_ = "ZERO";
        return;
      case KeyRole::Enter:
        matrix_stage_ = MatrixMenuStage::Cells;
        matrix_cell_row_ = 0;
        matrix_cell_col_ = 0;
        matrix_cell_editing_ = false;
        status_ = "MOVE CELL";
        return;
      case KeyRole::Clear:
        matrix_stage_ = MatrixMenuStage::Matrices;
        status_ = "PICK MATRIX";
        return;
      default:
        return;
    }
  }

  const uint8_t rows = matrix_rows_[matrix_selected_];
  const uint8_t cols = matrix_cols_[matrix_selected_];
  if (matrix_cell_row_ >= rows) {
    matrix_cell_row_ = rows > 0 ? rows - 1 : 0;
  }
  if (matrix_cell_col_ >= cols) {
    matrix_cell_col_ = cols > 0 ? cols - 1 : 0;
  }

  if (matrix_cell_editing_) {
    switch (def.role) {
      case KeyRole::Left:
        matrix_cell_editing_ = false;
        move_matrix_cell(0, -1);
        status_ = "MOVE CELL";
        return;
      case KeyRole::Right:
        matrix_cell_editing_ = false;
        move_matrix_cell(0, 1);
        status_ = "MOVE CELL";
        return;
      case KeyRole::Up:
        matrix_cell_editing_ = false;
        move_matrix_cell(-1, 0);
        status_ = "MOVE CELL";
        return;
      case KeyRole::Down:
        matrix_cell_editing_ = false;
        move_matrix_cell(1, 0);
        status_ = "MOVE CELL";
        return;
      case KeyRole::Enter:
        matrix_cell_editing_ = false;
        status_ = "MOVE CELL";
        return;
      case KeyRole::Delete:
      case KeyRole::Clear:
        if (key.shift) {
          matrix_cells_[matrix_selected_][matrix_cell_row_][matrix_cell_col_][0] = '\0';
        } else {
          delete_matrix_cell_char();
        }
        status_ = "EDIT CELL";
        return;
      default:
        break;
    }

    const char* token = key_input(def, key.shift, key.alpha);
    status_ = append_matrix_cell_token(token) ? "EDIT CELL" : "CELL FULL";
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
      move_matrix_cell(0, -1);
      status_ = "MOVE CELL";
      return;
    case KeyRole::Right:
      move_matrix_cell(0, 1);
      status_ = "MOVE CELL";
      return;
    case KeyRole::Up:
      move_matrix_cell(-1, 0);
      status_ = "MOVE CELL";
      return;
    case KeyRole::Down:
      move_matrix_cell(1, 0);
      status_ = "MOVE CELL";
      return;
    case KeyRole::Enter:
      if (key_is_equals(key)) {
        commit_matrix_definition();
      } else {
        matrix_cell_editing_ = true;
        cursor_visible_ = true;
        status_ = "EDIT CELL";
      }
      return;
    case KeyRole::FractionToggle:
      fill_matrix_identity(matrix_selected_);
      status_ = "IDENTITY";
      return;
    case KeyRole::Delete:
      matrix_cells_[matrix_selected_][matrix_cell_row_][matrix_cell_col_][0] = '\0';
      status_ = "CELL ZERO";
      return;
    case KeyRole::Clear:
      matrix_stage_ = MatrixMenuStage::Size;
      matrix_cell_editing_ = false;
      status_ = "PICK SIZE";
      return;
    default:
      break;
  }

  const char* token = key_input(def, key.shift, key.alpha);
  if (has_text(token)) {
    matrix_cell_editing_ = true;
    cursor_visible_ = true;
    status_ = append_matrix_cell_token(token) ? "EDIT CELL" : "CELL FULL";
  }
}

void MenuUi::move_matrix_selection(int delta) {
  int next = static_cast<int>(matrix_selected_) + delta;
  while (next < 0) {
    next += static_cast<int>(constants::kMatrixCount);
  }
  matrix_selected_ = static_cast<uint8_t>(next % static_cast<int>(constants::kMatrixCount));
}

void MenuUi::move_matrix_cell(int row_delta, int col_delta) {
  int row = static_cast<int>(matrix_cell_row_) + row_delta;
  int col = static_cast<int>(matrix_cell_col_) + col_delta;
  const int rows = std::max<int>(1, matrix_rows_[matrix_selected_]);
  const int cols = std::max<int>(1, matrix_cols_[matrix_selected_]);
  while (row < 0) {
    row += rows;
  }
  while (col < 0) {
    col += cols;
  }
  matrix_cell_row_ = static_cast<uint8_t>(row % rows);
  matrix_cell_col_ = static_cast<uint8_t>(col % cols);
}

void MenuUi::adjust_matrix_rows(int delta) {
  int rows = static_cast<int>(matrix_rows_[matrix_selected_]) + delta;
  rows = std::clamp(rows, 1, static_cast<int>(constants::kMatrixMaxRows));
  matrix_rows_[matrix_selected_] = static_cast<uint8_t>(rows);
  if (matrix_cell_row_ >= rows) {
    matrix_cell_row_ = static_cast<uint8_t>(rows - 1);
  }
}

void MenuUi::adjust_matrix_cols(int delta) {
  int cols = static_cast<int>(matrix_cols_[matrix_selected_]) + delta;
  cols = std::clamp(cols, 1, static_cast<int>(constants::kMatrixMaxCols));
  matrix_cols_[matrix_selected_] = static_cast<uint8_t>(cols);
  if (matrix_cell_col_ >= cols) {
    matrix_cell_col_ = static_cast<uint8_t>(cols - 1);
  }
}

void MenuUi::fill_matrix_zero(uint8_t matrix) {
  if (matrix >= constants::kMatrixCount) {
    return;
  }
  for (size_t row = 0; row < constants::kMatrixMaxRows; ++row) {
    for (size_t col = 0; col < constants::kMatrixMaxCols; ++col) {
      std::snprintf(matrix_cells_[matrix][row][col],
                    constants::kMatrixCellCapacity,
                    "0");
    }
  }
}

void MenuUi::fill_matrix_identity(uint8_t matrix) {
  if (matrix >= constants::kMatrixCount) {
    return;
  }
  fill_matrix_zero(matrix);
  const size_t diagonal = std::min<size_t>(matrix_rows_[matrix], matrix_cols_[matrix]);
  for (size_t i = 0; i < diagonal; ++i) {
    std::snprintf(matrix_cells_[matrix][i][i], constants::kMatrixCellCapacity, "1");
  }
}

bool MenuUi::append_matrix_cell_token(const char* token) {
  if (!has_text(token)) {
    return false;
  }
  char* cell = matrix_cells_[matrix_selected_][matrix_cell_row_][matrix_cell_col_];
  const size_t used = std::strlen(cell);
  const size_t add = std::strlen(token);
  if (used + add >= constants::kMatrixCellCapacity) {
    return false;
  }
  std::memcpy(cell + used, token, add + 1);
  return true;
}

void MenuUi::delete_matrix_cell_char() {
  char* cell = matrix_cells_[matrix_selected_][matrix_cell_row_][matrix_cell_col_];
  const size_t used = std::strlen(cell);
  if (used == 0) {
    return;
  }
  cell[used - 1] = '\0';
}

bool MenuUi::build_matrix_assignment(uint8_t matrix, char* output, size_t output_size) const {
  if (matrix >= constants::kMatrixCount || output_size == 0) {
    return false;
  }
  output[0] = '\0';
  if (!append_text(output, output_size, constants::kMatrixLabels[matrix]) ||
      !append_text(output, output_size, ":=[")) {
    return false;
  }

  const uint8_t rows = matrix_rows_[matrix];
  const uint8_t cols = matrix_cols_[matrix];
  for (uint8_t row = 0; row < rows; ++row) {
    if (row > 0 && !append_text(output, output_size, ",")) {
      return false;
    }
    if (!append_text(output, output_size, "[")) {
      return false;
    }
    for (uint8_t col = 0; col < cols; ++col) {
      if (col > 0 && !append_text(output, output_size, ",")) {
        return false;
      }
      const char* cell = matrix_cells_[matrix][row][col];
      if (!append_text(output, output_size, has_text(cell) ? cell : "0")) {
        return false;
      }
    }
    if (!append_text(output, output_size, "]")) {
      return false;
    }
  }
  return append_text(output, output_size, "]");
}

void MenuUi::commit_matrix_definition() {
  MathRequest request {};
  request.kind = MathJobKind::Script;
  // Save sends bounded `A:=[[...]]` script to Giac, then Standard shows only
  // matrix name so input area is not consumed by assignment text.
  if (!build_matrix_assignment(matrix_selected_, request.expression, sizeof(request.expression))) {
    status_ = "MATRIX BIG";
    return;
  }
  if (!math_.submit(request)) {
    status_ = "MATH BUSY";
    return;
  }

  const char* matrix_name = constants::kMatrixLabels[matrix_selected_];
  open_mode(ModeKind::Standard);
  std::snprintf(expression_, sizeof(expression_), "%s", matrix_name);
  expression_len_ = std::strlen(expression_);
  cursor_ = expression_len_;
  clear_result();
  status_ = "MATRIX SENT";
}

void MenuUi::insert_matrix_name() {
  const bool inserted = append_expression(constants::kMatrixLabels[matrix_selected_]);
  open_mode(ModeKind::Standard);
  status_ = inserted ? "MATRIX NAME" : "EXPR FULL";
}

void MenuUi::render_matrix() {
  const char* matrix_name = constants::kMatrixLabels[matrix_selected_];

  if (matrix_stage_ == MatrixMenuStage::Matrices) {
    canvas_.draw_text(6, 18, "CHOOSE MATRIX", 1, true);
    for (size_t i = 0; i < constants::kMatrixCount; ++i) {
      const int x = 18 + static_cast<int>(i) * 37;
      const bool selected = i == matrix_selected_;
      if (selected) {
        canvas_.rect(x - 7, 39, 24, 22, true);
        canvas_.rect(x - 5, 41, 20, 18, true);
      }
      canvas_.draw_text(x, 47, constants::kMatrixLabels[i], 1, true);
      char index_text[4] {};
      std::snprintf(index_text, sizeof(index_text), "%u", static_cast<unsigned>(i));
      canvas_.draw_text(x, 66, index_text, 1, true);
    }
    canvas_.draw_text(6, 116, "LR CHOOSE  CALC SIZE  = NAME", 1, true);
    return;
  }

  if (matrix_stage_ == MatrixMenuStage::Size) {
    char title[24] {};
    std::snprintf(title, sizeof(title), "%s DIMENSIONS", matrix_name);
    canvas_.draw_text(6, 18, title, 1, true);

    char size_text[16] {};
    std::snprintf(size_text,
                  sizeof(size_text),
                  "%u x %u",
                  static_cast<unsigned>(matrix_rows_[matrix_selected_]),
                  static_cast<unsigned>(matrix_cols_[matrix_selected_]));
    canvas_.draw_text(86, 51, size_text, 2, true);
    canvas_.draw_text(26, 83, "UP/DOWN ROWS   LEFT/RIGHT COLS", 1, true);
    canvas_.draw_text(6, 116, "1-4 SQUARE  S<>D I  = EDIT", 1, true);
    return;
  }

  char title[24] {};
  std::snprintf(title,
                sizeof(title),
                "%s %ux%u",
                matrix_name,
                static_cast<unsigned>(matrix_rows_[matrix_selected_]),
                static_cast<unsigned>(matrix_cols_[matrix_selected_]));
  canvas_.draw_text(6, 18, title, 1, true);

  const int grid_x = 8;
  const int grid_y = 34;
  const int grid_w = 238;
  const int grid_h = 70;
  const int rows = std::max<int>(1, matrix_rows_[matrix_selected_]);
  const int cols = std::max<int>(1, matrix_cols_[matrix_selected_]);
  const int cell_w = grid_w / cols;
  const int cell_h = grid_h / rows;

  canvas_.rect(grid_x, grid_y, cell_w * cols + 1, cell_h * rows + 1, true);
  for (int col = 1; col < cols; ++col) {
    canvas_.vline(grid_x + col * cell_w, grid_y, cell_h * rows + 1, true);
  }
  for (int row = 1; row < rows; ++row) {
    canvas_.hline(grid_x, grid_y + row * cell_h, cell_w * cols + 1, true);
  }

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const int x = grid_x + col * cell_w;
      const int y = grid_y + row * cell_h;
      const bool selected = row == matrix_cell_row_ && col == matrix_cell_col_;
      if (selected) {
        canvas_.rect(x + 2, y + 2, cell_w - 3, cell_h - 3, true);
      }

      const size_t max_chars = static_cast<size_t>(std::max(1, (cell_w - 8) / constants::kCharWidth));
      char text[constants::kMatrixCellCapacity] {};
      visible_cell_text(matrix_cells_[matrix_selected_][row][col],
                        text,
                        sizeof(text),
                        max_chars);
      const int text_x = x + 5;
      const int text_y = y + std::max(4, (cell_h - 7) / 2);
      canvas_.draw_text(text_x, text_y, text, 1, true);

      if (selected && matrix_cell_editing_ && cursor_visible_) {
        const int used_chars = static_cast<int>(std::min(std::strlen(text), max_chars));
        int cursor_x = text_x + used_chars * constants::kCharWidth + 1;
        cursor_x = std::min(cursor_x, x + cell_w - 4);
        canvas_.vline(cursor_x, y + 4, cell_h - 7, true);
      }
    }
  }

  canvas_.draw_text(6, 116, "ARROWS MOVE  CALC EDIT  = SAVE", 1, true);
}

}  // namespace esp32calc_alt
