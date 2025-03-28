#include "utils/ignore_filter.h"

namespace ignore_filter {

void ignore_filter_pattern::load_pattern(const std::string &pattern_yml) {
  YAML::Node ignore_filter_pattern;
  try {
    ignore_filter_pattern = YAML::LoadFile(pattern_yml);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::exit(1);
  }
  for (const auto &pattern : ignore_filter_pattern) {
    auto p = std::make_unique<RE2>(pattern.as<std::string>());
    _patterns.push_back(std::move(p));
  }
  fmt::print(
      "ignore_filter match enabled, finsh loading ignore_filter pattern from "
      "{}\n",
      pattern_yml);
  _enable_ignore_filter = true;
}

bool ignore_filter_pattern::check_ignore_filter(const std::string &line) const {
  if (!_enable_ignore_filter) {
    return false;
  }
  for (const auto &pattern : _patterns) {
    // Check if the line matches any of the patterns
    if (RE2::PartialMatch(line, *pattern)) {
      return true;  // If any pattern matches, return true
    }
  }
  return false;
}

}  // namespace ignore_filter
