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
    _pattern = std::make_unique<RE2>(pattern.as<std::string>());
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
  if (RE2::PartialMatch(line, *_pattern)) {
    return true;
  }
  return false;
}

}  // namespace ignore_filter
