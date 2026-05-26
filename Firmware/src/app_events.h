#pragma once

#include <cstdint>

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

struct CalcResult {
  bool is_error_;
  int num_of_terms;
  MathTerm* terms;

  // generic constructor, good to have since it can be called
  CalcResult() : is_error_(false), num_of_terms(0), terms(nullptr) {}

  // we can use the good constructor if we did not get an error
  CalcResult(int num_terms, bool error = false) {
    this->is_error_ = error;
    this->num_of_terms = num_terms; 

    if (num_terms > 0) terms = new MathTerm[num_terms];
    else terms = nullptr;
  }

  // now we get rid of the passed terms
  void free_memory() {
    if (terms) {
      for (int i = 0; i < num_of_terms; ++i) {
      terms[i].free_memory();
    }
    delete[] terms;
    terms = nullptr;
    };
    num_of_terms = 0;
  }

};

struct AppEvent {
  AppEventType type = AppEventType::Key;
  KeyEvent key {};
  BatterySnapshot battery {};
  CalcResult calc {};
};

}  // namespace esp32calc
