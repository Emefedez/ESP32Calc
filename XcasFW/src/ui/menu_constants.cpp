#include "ui/menu_constants.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace esp32calc_alt::menu_constants {
namespace {

bool label_equals(const char* label, const char* text, size_t length) {
  return label != nullptr && std::strlen(label) == length &&
         std::strncmp(label, text, length) == 0;
}

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

char lower_ascii(char value) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

bool contains_case_insensitive(const char* haystack, const char* needle) {
  if (!has_text(needle)) {
    return true;
  }
  if (!has_text(haystack)) {
    return false;
  }

  const size_t needle_len = std::strlen(needle);
  for (size_t i = 0; haystack[i] != '\0'; ++i) {
    size_t matched = 0;
    while (matched < needle_len && haystack[i + matched] != '\0' &&
           lower_ascii(haystack[i + matched]) == lower_ascii(needle[matched])) {
      ++matched;
    }
    if (matched == needle_len) {
      return true;
    }
  }
  return false;
}

bool query_is_digits(const char* query) {
  if (!has_text(query)) {
    return false;
  }
  for (size_t i = 0; query[i] != '\0'; ++i) {
    if (std::isdigit(static_cast<unsigned char>(query[i])) == 0) {
      return false;
    }
  }
  return true;
}

bool constant_code_matches(size_t index, const char* query) {
  char code[4] {};
  std::snprintf(code, sizeof(code), "%02u", static_cast<unsigned>(index));
  return std::strncmp(code, query, std::strlen(query)) == 0;
}

bool append_text(char* output, size_t output_size, size_t& used, const char* text) {
  if (output == nullptr || output_size == 0 || text == nullptr) {
    return false;
  }
  const int written = std::snprintf(output + used, output_size - used, "%s", text);
  if (written < 0 || static_cast<size_t>(written) >= output_size - used) {
    return false;
  }
  used += static_cast<size_t>(written);
  return true;
}

bool append_char(char* output, size_t output_size, size_t& used, char value) {
  if (output == nullptr || used + 1 >= output_size) {
    return false;
  }
  output[used++] = value;
  output[used] = '\0';
  return true;
}

}  // namespace

const ScientificConstant* find_scientific_constant(const char* label, size_t length) {
  if (label == nullptr || length == 0) {
    return nullptr;
  }
  for (const auto& constant : kScientificConstants) {
    if (label_equals(constant.label, label, length)) {
      return &constant;
    }
  }
  return nullptr;
}

bool expand_scientific_constants(const char* input, char* output, size_t output_size) {
  if (output == nullptr || output_size == 0) {
    return false;
  }
  output[0] = '\0';
  if (input == nullptr) {
    return true;
  }

  size_t used = 0;
  for (size_t i = 0; input[i] != '\0';) {
    if (input[i] != kConstantMarker) {
      if (!append_char(output, output_size, used, input[i])) {
        return false;
      }
      ++i;
      continue;
    }

    const size_t label_begin = i + 1;
    size_t label_end = label_begin;
    while (std::isalnum(static_cast<unsigned char>(input[label_end])) != 0) {
      ++label_end;
    }

    const ScientificConstant* constant =
        find_scientific_constant(input + label_begin, label_end - label_begin);
    if (constant == nullptr) {
      if (!append_char(output, output_size, used, input[i])) {
        return false;
      }
      ++i;
      continue;
    }

    if (!append_char(output, output_size, used, '(') ||
        !append_text(output, output_size, used, constant->value) ||
        !append_char(output, output_size, used, ')')) {
      return false;
    }
    i = label_end;
  }
  return true;
}

const ScientificConstantGroup& constant_group_at(size_t index) {
  return kScientificConstantGroups[index];
}

bool scientific_constant_matches(const ScientificConstant& item,
                                 size_t index,
                                 uint8_t group,
                                 const char* query) {
  if (group >= kScientificConstantGroupCount) {
    return false;
  }
  if (std::strcmp(item.category, kScientificConstantGroups[group].category) != 0) {
    return false;
  }
  if (!has_text(query)) {
    return true;
  }
  if (query_is_digits(query)) {
    return constant_code_matches(index, query);
  }
  return contains_case_insensitive(item.label, query) ||
         contains_case_insensitive(item.category, query) ||
         contains_case_insensitive(item.value, query);
}

size_t filtered_scientific_constant_count(uint8_t group, const char* query) {
  size_t count = 0;
  for (size_t i = 0; i < kScientificConstantCount; ++i) {
    if (scientific_constant_matches(kScientificConstants[i], i, group, query)) {
      ++count;
    }
  }
  return count;
}

int filtered_scientific_constant_index(uint8_t group, const char* query, size_t ordinal) {
  size_t seen = 0;
  for (size_t i = 0; i < kScientificConstantCount; ++i) {
    if (!scientific_constant_matches(kScientificConstants[i], i, group, query)) {
      continue;
    }
    if (seen == ordinal) {
      return static_cast<int>(i);
    }
    ++seen;
  }
  return -1;
}

int first_scientific_constant_for_group(uint8_t group) {
  return filtered_scientific_constant_index(group, "", 0);
}

void sanitize_constant_search_token(const char* token, char* output, size_t output_size) {
  if (output == nullptr || output_size == 0) {
    return;
  }
  output[0] = '\0';
  if (token == nullptr) {
    return;
  }

  size_t used = 0;
  for (size_t i = 0; token[i] != '\0' && used + 1 < output_size; ++i) {
    const unsigned char value = static_cast<unsigned char>(token[i]);
    if (std::isalnum(value) != 0) {
      output[used++] = static_cast<char>(std::tolower(value));
    }
  }
  output[used] = '\0';
}

}  // namespace esp32calc_alt::menu_constants
