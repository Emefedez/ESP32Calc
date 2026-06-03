#include "ui/menu_solvers.h"

#include <cctype>
#include <cstring>

namespace esp32calc_alt::menu_solvers {
namespace {

inline constexpr SolverGroup kGroups[] = {
    {"EQUATIONS", "solve"},
    {"SISTEMAS", "2x2/3x3"},
    {"POLY", "roots"},
    {"NUMERIC", "fsolve"},
    {"LINEAR", "matrix"},
};

inline constexpr SolverTemplate kTemplates[] = {
    {0, "00", "Solve for x", "solve(,x)", 6, "equation x variable"},
    {0, "01", "Solve for y", "solve(,y)", 6, "equation y variable"},
    {0, "02", "Solve x,y", "solve(,[x,y])", 6, "two variables"},
    {0, "03", "Isolate expression", "solve(=,x)", 6, "isolate equation"},
    {1, "10", "Sistema 2x2", "sistemas(x+y=;x-y=,[x,y])", 12, "sistema ecuaciones"},
    {1, "11", "Sistema 3x3", "sistemas(x+y+z=;x-y+z=;x+y-z=,[x,y,z])", 14, "sistema ecuaciones"},
    {1, "12", "Blank 2 eq", "sistemas(=;=,[x,y])", 10, "sistema blank"},
    {1, "13", "Blank 3 eq", "sistemas(=;=;=,[x,y,z])", 10, "sistema blank"},
    {2, "20", "Quadratic roots", "solve(a*x^2+b*x+c=0,x)", 6, "polynomial quadratic"},
    {2, "21", "Polynomial roots", "solve(poly1([,,]),x)", 12, "polynomial coefficients"},
    {2, "22", "Factor then solve", "solve(factor()=0,x)", 13, "factor roots"},
    {3, "30", "Numeric solve", "fsolve(=,x)", 7, "numeric approximate"},
    {3, "31", "Numeric interval", "fsolve(,x=..)", 7, "numeric interval"},
    {4, "40", "Linear solve", "linsolve(,)", 9, "linear algebra matrix"},
    {4, "41", "A*x=b", "linsolve(A,B)", 11, "matrix linear"},
};

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

bool starts_with(const char* text, const char* prefix) {
  return has_text(text) && has_text(prefix) &&
         std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

}  // namespace

size_t group_count() {
  return sizeof(kGroups) / sizeof(kGroups[0]);
}

const SolverGroup& group_at(size_t index) {
  return kGroups[index];
}

size_t template_count() {
  return sizeof(kTemplates) / sizeof(kTemplates[0]);
}

const SolverTemplate& template_at(size_t index) {
  return kTemplates[index];
}

bool template_matches(const SolverTemplate& item, uint8_t group, const char* query) {
  if (item.group != group) {
    return false;
  }
  if (!has_text(query)) {
    return true;
  }
  return starts_with(item.code, query) || contains_case_insensitive(item.label, query) ||
         contains_case_insensitive(item.token, query) ||
         contains_case_insensitive(item.keywords, query);
}

size_t filtered_template_count(uint8_t group, const char* query) {
  size_t count = 0;
  for (size_t i = 0; i < template_count(); ++i) {
    if (template_matches(kTemplates[i], group, query)) {
      ++count;
    }
  }
  return count;
}

int filtered_template_index(uint8_t group, const char* query, size_t ordinal) {
  size_t seen = 0;
  for (size_t i = 0; i < template_count(); ++i) {
    if (!template_matches(kTemplates[i], group, query)) {
      continue;
    }
    if (seen == ordinal) {
      return static_cast<int>(i);
    }
    ++seen;
  }
  return -1;
}

int first_template_for_group(uint8_t group) {
  return filtered_template_index(group, "", 0);
}

void sanitize_search_token(const char* token, char* output, size_t output_size) {
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

}  // namespace esp32calc_alt::menu_solvers
