#pragma once

#include <cstddef>
#include <cstdint>

namespace esp32calc_alt::menu_integrals {

struct IntegralGroup {
  const char* label;
  const char* hint;
};

struct IntegralTemplate {
  uint8_t group;
  const char* code;
  const char* label;
  const char* token;
  size_t cursor;
  const char* keywords;
};

size_t group_count();
const IntegralGroup& group_at(size_t index);
size_t template_count();
const IntegralTemplate& template_at(size_t index);
bool template_matches(const IntegralTemplate& item, uint8_t group, const char* query);
size_t filtered_template_count(uint8_t group, const char* query);
int filtered_template_index(uint8_t group, const char* query, size_t ordinal);
int first_template_for_group(uint8_t group);
void sanitize_search_token(const char* token, char* output, size_t output_size);

}  // namespace esp32calc_alt::menu_integrals
