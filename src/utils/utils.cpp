#include "utils.h"

std::vector<std::string_view> split_string_by_spaces(const std::string &str) {
  std::vector<std::string_view> result;
  std::string_view str_view = str;
  size_t start = 0;
  size_t end = 0;

  while (end < str_view.size()) {
    // Find the start of the next word
    while (start < str_view.size() && std::isspace(str_view[start])) {
      ++start;
    }

    // Find the end of the word
    end = start;
    while (end < str_view.size() && !std::isspace(str_view[end])) {
      ++end;
    }

    // If start is less than end, we have a word
    if (start < end) {
      result.emplace_back(str_view.substr(start, end - start));
      start = end;
    }
  }

  return result;
}
