#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "math/types.h"

namespace esp32calc_alt::giac {

enum class GiacOperation : uint8_t {
  Evaluate,
  Simplify,
  Solve,
  Matrix,
  Graph,
  Raw,
};

struct GiacBridgeConfig {
  bool exact = true;
  bool allow_complex = true;
  bool keep_steps = false;
};

struct GiacResponse {
  bool ok = false;
  GiacOperation operation = GiacOperation::Raw;
  char command[256] {};
  char plain[192] {};
  char error[96] {};
};

struct GiacGraphResponse {
  bool ok = false;
  char command[256] {};
  char error[96] {};
  float y[kGraphSampleCount] {};
  bool valid[kGraphSampleCount] {};
  size_t count = 0;
};

class GiacBridge {
 public:
  ~GiacBridge();

  GiacBridge() = default;
  GiacBridge(const GiacBridge&) = delete;
  GiacBridge& operator=(const GiacBridge&) = delete;

  esp_err_t begin(const GiacBridgeConfig& config = {});
  bool available() const;
  const char* status_text() const;
  void reset_context();

  GiacResponse evaluate(const char* expression);
  GiacResponse simplify(const char* expression);
  GiacResponse solve(const char* equation, const SolveOptions& options = {});
  GiacResponse matrix(const char* expression);
  GiacResponse determinant(const char* matrix_expression);
  GiacResponse inverse(const char* matrix_expression);
  GiacResponse graph_expression(const char* expression);
  GiacGraphResponse sample_graph(const char* expression,
                                 float x_min,
                                 float x_max,
                                 size_t sample_count);
  GiacResponse raw(const char* command);

 private:
  GiacResponse run(GiacOperation operation, const char* command);
  static char first_solve_variable(const SolveOptions& options);
  static void format_solve_variables(const SolveOptions& options,
                                     char* output,
                                     size_t output_size);
  static bool format_system_equations(const char* equations, char* output, size_t output_size);

  GiacBridgeConfig config_ {};
  bool ready_ = false;
  esp_err_t status_ = ESP_ERR_INVALID_STATE;
  void* context_ = nullptr;
};

}  // namespace esp32calc_alt::giac
