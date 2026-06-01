#include "ui/menu_integrals.h"

#include <cctype>
#include <cstring>

namespace esp32calc_alt::menu_integrals {
namespace {

inline constexpr IntegralGroup kGroups[] = {
    {"BASIC", "int"},
    {"DEFINITE", "bounds"},
    {"TRIG", "sin/cos"},
    {"EXP LOG", "e/ln"},
    {"SPECIAL", "parts"},
};

inline constexpr IntegralTemplate kTemplates[] = {
    {0, "00", "Indefinite integral", "int(,x)", 4, "primitive antiderivative"},
    {0, "01", "Integrate command", "integrate(,x)", 10, "primitive antiderivative"},
    {0, "02", "Power rule", "int(x^,x)", 6, "power polynomial"},
    {0, "03", "Reciprocal", "int(1/x,x)", 10, "reciprocal logarithm"},
    {0, "04", "Sqrt", "int(sqrt(x),x)", 13, "root radical"},
    {1, "10", "Definite integral", "int(,x,,)", 4, "bounds interval"},
    {1, "11", "0 to 1", "int(,x,0,1)", 4, "bounds unit"},
    {1, "12", "0 to pi", "int(,x,0,pi)", 4, "bounds pi"},
    {1, "13", "-inf to inf", "int(,x,-inf,inf)", 4, "improper infinity"},
    {1, "14", "Numeric approx", "evalf(int(,x,,))", 10, "decimal approximate"},
    {2, "20", "sin(x)", "int(sin(x),x)", 13, "trigonometric sine"},
    {2, "21", "cos(x)", "int(cos(x),x)", 13, "trigonometric cosine"},
    {2, "22", "tan(x)", "int(tan(x),x)", 13, "trigonometric tangent"},
    {2, "23", "sec^2(x)", "int(1/(cos(x)^2),x)", 18, "trigonometric secant"},
    {2, "24", "1/(1+x^2)", "int(1/(1+x^2),x)", 17, "arctan inverse"},
    {2, "25", "1/sqrt(1-x^2)", "int(1/sqrt(1-x^2),x)", 22, "arcsin inverse"},
    {3, "30", "exp(x)", "int(exp(x),x)", 13, "exponential"},
    {3, "31", "a^x", "int(a^x,x)", 10, "exponential base"},
    {3, "32", "ln(x)", "int(ln(x),x)", 12, "logarithm natural"},
    {3, "33", "x*ln(x)", "int(x*ln(x),x)", 14, "logarithm parts"},
    {3, "34", "1/(a*x+b)", "int(1/(a*x+b),x)", 17, "linear denominator"},
    {4, "40", "By parts shell", "int(*,x)", 4, "parts product"},
    {4, "41", "Substitution shell", "subst(int(,x),x=)", 10, "change variable"},
    {4, "42", "Partial fractions", "partfrac()", 9, "rational decomposition"},
    {4, "43", "Assume positive", "assume(x>0);int(,x)", 18, "domain"},
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

const IntegralGroup& group_at(size_t index) {
  return kGroups[index];
}

size_t template_count() {
  return sizeof(kTemplates) / sizeof(kTemplates[0]);
}

const IntegralTemplate& template_at(size_t index) {
  return kTemplates[index];
}

bool template_matches(const IntegralTemplate& item, uint8_t group, const char* query) {
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

}  // namespace esp32calc_alt::menu_integrals
