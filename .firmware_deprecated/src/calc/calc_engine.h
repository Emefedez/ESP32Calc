#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace esp32calc {

struct CalcResult;

enum class CalcRequestKind : uint8_t {
  Evaluate,
  Graph,
  Solve,
};

inline constexpr size_t kCalcSolveVariableCount = 6;
inline constexpr uint8_t kCalcSolveVariableX = 1u << 0;
inline constexpr uint8_t kCalcSolveVariableY = 1u << 1;
inline constexpr uint8_t kCalcSolveVariableZ = 1u << 2;
inline constexpr uint8_t kCalcSolveVariableA = 1u << 3;
inline constexpr uint8_t kCalcSolveVariableB = 1u << 4;
inline constexpr uint8_t kCalcSolveVariableC = 1u << 5;

struct CalcSolveOptions {
  uint8_t solve_mask = 0; //the solve_mask identifies what variables we want to solve for, by default, it goes in appearance order.
  uint8_t parameter_mask = 0; // parameters are unsolved variables, so it is masked sepparately
  uint8_t known_mask = 0; // this is trated as a helper, these values are already "OK", so we compare what we have to what we want to get
  double known_values[kCalcSolveVariableCount] {};
};

struct CalcRequest {
  CalcRequestKind kind = CalcRequestKind::Evaluate; //default state, calc_engine.cpp will see if it should change
  char* dynamic_expression = nullptr;
  CalcSolveOptions solve_options {};
};

enum class CalcExpressionKind : uint8_t {
  Invalid,
  Numeric,
  Symbolic,
  Equation,
};

class CalcEngine {
 public:
  esp_err_t start(QueueHandle_t app_events);
  bool submit_expression(const char* expr);
  bool submit_solve_expression(const char* expr, CalcSolveOptions options = {});
  bool submit_graph_expression(const char* expr);
  CalcExpressionKind expression_identifier(const char* expr);

 private:
  static void task_trampoline(void* arg);
  void task();
  bool submit_request(CalcRequestKind kind,
                      const char* expr,
                      const CalcSolveOptions& options = {});
  void publish_result(CalcResult* result);

  QueueHandle_t app_events_ = nullptr;
  QueueHandle_t requests_queue_ = nullptr;
};

}  // namespace esp32calc
