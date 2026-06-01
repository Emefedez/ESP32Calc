#include "math/solve/solve_result_formatter.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace esp32calc_alt::solve {
namespace {

std::string trim_copy(const std::string& input) {
  size_t begin = 0;
  while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
    ++begin;
  }
  size_t end = input.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }
  return input.substr(begin, end - begin);
}

bool starts_with_ignore_case(const std::string& value, const char* prefix) {
  const size_t prefix_len = std::strlen(prefix);
  if (value.size() < prefix_len) {
    return false;
  }
  for (size_t i = 0; i < prefix_len; ++i) {
    const unsigned char left = static_cast<unsigned char>(value[i]);
    const unsigned char right = static_cast<unsigned char>(prefix[i]);
    if (std::tolower(left) != std::tolower(right)) {
      return false;
    }
  }
  return true;
}

bool has_outer_pair(const std::string& value, char open, char close) {
  return value.size() >= 2 && value[0] == open && value[value.size() - 1] == close;
}

bool contains_top_level_equals(const std::string& value) {
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  for (char ch : value) {
    switch (ch) {
      case '(':
        ++paren_depth;
        break;
      case ')':
        if (paren_depth > 0) {
          --paren_depth;
        }
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        if (bracket_depth > 0) {
          --bracket_depth;
        }
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        if (brace_depth > 0) {
          --brace_depth;
        }
        break;
      case '=':
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          return true;
        }
        break;
      default:
        break;
    }
  }
  return false;
}

std::string strip_collection_wrapper(const std::string& printed) {
  const std::string value = trim_copy(printed);
  if (starts_with_ignore_case(value, "list[") && has_outer_pair(value.substr(4), '[', ']')) {
    return trim_copy(value.substr(5, value.size() - 6));
  }
  if (has_outer_pair(value, '[', ']')) {
    return trim_copy(value.substr(1, value.size() - 2));
  }
  return value;
}

void split_top_level_items(const std::string& value, std::vector<std::string>& items) {
  items.clear();
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  size_t item_begin = 0;

  for (size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    switch (ch) {
      case '(':
        ++paren_depth;
        break;
      case ')':
        if (paren_depth > 0) {
          --paren_depth;
        }
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        if (bracket_depth > 0) {
          --bracket_depth;
        }
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        if (brace_depth > 0) {
          --brace_depth;
        }
        break;
      case ',':
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          items.push_back(trim_copy(value.substr(item_begin, i - item_begin)));
          item_begin = i + 1;
        }
        break;
      default:
        break;
    }
  }

  items.push_back(trim_copy(value.substr(item_begin)));
}

bool is_known_solve_variable(char variable) {
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if (kSolveVariables[i] == variable) {
      return true;
    }
  }
  return false;
}

bool extract_solve_target(const char* solve_command, std::string& target) {
  const std::string command = trim_copy(solve_command == nullptr ? "" : solve_command);
  if (!starts_with_ignore_case(command, "solve(") || !has_outer_pair(command.substr(5), '(', ')')) {
    return false;
  }

  const std::string args = command.substr(6, command.size() - 7);
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  size_t target_begin = std::string::npos;

  for (size_t i = 0; i < args.size(); ++i) {
    const char ch = args[i];
    switch (ch) {
      case '(':
        ++paren_depth;
        break;
      case ')':
        if (paren_depth > 0) {
          --paren_depth;
        }
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        if (bracket_depth > 0) {
          --bracket_depth;
        }
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        if (brace_depth > 0) {
          --brace_depth;
        }
        break;
      case ',':
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          target_begin = i + 1;
        }
        break;
      default:
        break;
    }
  }

  if (target_begin == std::string::npos || target_begin >= args.size()) {
    return false;
  }
  target = trim_copy(args.substr(target_begin));
  return !target.empty();
}

std::vector<char> selected_variables_from_command(const char* solve_command) {
  std::string target;
  if (!extract_solve_target(solve_command, target)) {
    return {};
  }

  std::vector<char> variables;
  if (has_outer_pair(target, '[', ']')) {
    std::vector<std::string> names;
    split_top_level_items(target.substr(1, target.size() - 2), names);
    for (const std::string& name : names) {
      const std::string trimmed = trim_copy(name);
      if (trimmed.size() == 1 && is_known_solve_variable(trimmed[0])) {
        variables.push_back(trimmed[0]);
      }
    }
  } else if (target.size() == 1 && is_known_solve_variable(target[0])) {
    variables.push_back(target[0]);
  }
  return variables;
}

std::vector<char> selected_variables_from_options(const SolveOptions& options) {
  uint8_t mask = options.solve_mask;
  if (mask == 0) {
    mask = kSolveVariableX;
  }

  std::vector<char> variables;
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if ((mask & (1u << i)) != 0) {
      variables.push_back(kSolveVariables[i]);
    }
  }
  if (variables.empty()) {
    variables.push_back('x');
  }
  return variables;
}

std::vector<char> selected_variables(const char* solve_command, const SolveOptions& options) {
  std::vector<char> variables = selected_variables_from_command(solve_command);
  if (!variables.empty()) {
    return variables;
  }
  return selected_variables_from_options(options);
}

std::string strip_outer_brackets_once(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (has_outer_pair(trimmed, '[', ']')) {
    return trim_copy(trimmed.substr(1, trimmed.size() - 2));
  }
  return trimmed;
}

std::string format_solution_item(const std::string& item, const std::vector<char>& variables) {
  const std::string trimmed = trim_copy(item);
  if (trimmed.empty()) {
    return "";
  }

  if (contains_top_level_equals(trimmed)) {
    return strip_outer_brackets_once(trimmed);
  }

  if (variables.size() <= 1) {
    return std::string(1, variables.empty() ? 'x' : variables[0]) + "=" + trimmed;
  }

  const std::string vector_text = strip_outer_brackets_once(trimmed);
  std::vector<std::string> values;
  split_top_level_items(vector_text, values);
  if (values.size() != variables.size()) {
    return trimmed;
  }

  std::string formatted;
  for (size_t i = 0; i < values.size(); ++i) {
    if (!formatted.empty()) {
      formatted += ", ";
    }
    formatted.push_back(variables[i]);
    formatted += "=";
    formatted += values[i];
  }
  return formatted;
}

}  // namespace

SolvePresentation present_giac_solve_result(const char* cas_expression,
                                            const char* solve_command,
                                            const SolveOptions& options) {
  SolvePresentation presentation {};
  const std::string collection =
      strip_collection_wrapper(cas_expression == nullptr ? "" : cas_expression);
  if (collection.empty()) {
    std::snprintf(presentation.display, sizeof(presentation.display), "NO SOLUTIONS");
    return presentation;
  }

  const std::vector<char> variables = selected_variables(solve_command, options);
  std::vector<std::string> items;
  split_top_level_items(collection, items);

  std::string formatted;
  for (const std::string& item : items) {
    const std::string solution = format_solution_item(item, variables);
    if (solution.empty()) {
      continue;
    }
    if (!formatted.empty()) {
      formatted += "; ";
    }
    formatted += solution;
    if (presentation.solution_count < UINT8_MAX) {
      ++presentation.solution_count;
    }
  }

  if (formatted.empty()) {
    formatted = "NO SOLUTIONS";
  }
  std::snprintf(presentation.display, sizeof(presentation.display), "%s", formatted.c_str());
  return presentation;
}

}  // namespace esp32calc_alt::solve
