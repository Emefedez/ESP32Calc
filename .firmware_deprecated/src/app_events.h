#pragma once

#include <cstdint>
#include <new>

namespace esp32calc {

enum class AppEventType : uint8_t {
  Key,
  Battery,
  Storage,
  CalcResult,
  Wireless,
};

enum class KeyPhase : uint8_t {
  Pressed,
  Released,
};

// key + modifiers
struct KeyEvent {
  uint8_t row = 0;
  uint8_t col = 0;
  const char* label = "";
  KeyPhase phase = KeyPhase::Pressed;
  bool shift = false;
  bool alpha = false;
};

struct BatterySnapshot {
  uint16_t adc_mv = 0;
  uint16_t pack_mv = 0;
  uint8_t percent = 0;
};
struct MathTerm {
  float coefficient;
  char* variables; // character pointer, this points to the start of the list of possibly multiple variables

  // default constructor
  MathTerm() : coefficient(0.0f), variables(nullptr) {};

  // recommended practice to have a different one per struct, cleaner even if a bit verbose
  void free_memory() {
    if (variables) {
      delete[] variables;
      variables = nullptr;
    }
  }

};

enum class CalcResultAction : uint8_t {
  ShowText,
  OpenGraph,
};

struct CalcResult {
  bool is_error_;
  CalcResultAction action;
  char* display_text;
  char* graph_expression;
  int num_of_terms;
  MathTerm* terms;

  // generic constructor, good to have since it can be called
  CalcResult()
      : is_error_(false),
        action(CalcResultAction::ShowText),
        display_text(nullptr),
        graph_expression(nullptr),
        num_of_terms(0),
        terms(nullptr) {}

  // we can use the good constructor if we did not get an error
  CalcResult(int num_terms, bool error = false)
      : is_error_(error),
        action(CalcResultAction::ShowText),
        display_text(nullptr),
        graph_expression(nullptr),
        num_of_terms(num_terms),
        terms(nullptr) {
    if (num_terms > 0) {
      terms = new (std::nothrow) MathTerm[num_terms];
      if (terms == nullptr) {
        is_error_ = true;
        num_of_terms = 0;
      }
    }
  }

  // now we get rid of the passed terms
  void free_memory() {
    if (terms) {
      for (int i = 0; i < num_of_terms; ++i) {
        terms[i].free_memory();
      }
      delete[] terms;
      terms = nullptr;
    }
    num_of_terms = 0;
    if (display_text) {
      delete[] display_text;
      display_text = nullptr;
    }
    if (graph_expression) {
      delete[] graph_expression;
      graph_expression = nullptr;
    }
  }

};

inline void destroy_calc_result(CalcResult* result) {
  if (result == nullptr) {
    return;
  }
  result->free_memory();
  delete result;
}

struct AppEvent {
  AppEventType type = AppEventType::Key;
  KeyEvent key {};
  BatterySnapshot battery {};
  // Dynamic results travel by pointer because FreeRTOS queues copy event bytes.
  CalcResult* calc_result = nullptr;
};

}  // namespace esp32calc
