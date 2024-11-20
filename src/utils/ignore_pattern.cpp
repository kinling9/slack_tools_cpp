#include "ignore_pattern.h"

#include <fmt/core.h>

#include <iostream>

#include "yaml-cpp/yaml.h"

void ignore_pattern::load_pattern(const std::string &pattern_yml) {
  YAML::Node ignore_pattern;
  try {
    ignore_pattern = YAML::LoadFile(pattern_yml);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::exit(1);
  }
  for (const auto &tool_pattern : ignore_pattern) {
    for (const auto &pattern : tool_pattern.second) {
      std::unique_ptr<RE2> pattern_split =
          std::make_unique<RE2>(pattern.as<std::string>());
      _patterns[tool_pattern.first.as<std::string>()].emplace_back(
          std::move(pattern_split));
    }
  }
  fmt::print("ignore match enabled, finsh loading ignore pattern from {}\n",
             pattern_yml);
  _enable_ignore = true;
}

bool ignore_pattern::check_ignore(const std::string &tool,
                                  const std::string &line) const {
  if (!_enable_ignore) {
    return false;
  }
  if (!_patterns.contains(tool)) {
    return false;
  }
  for (const auto &pattern : _patterns.at(tool)) {
    if (RE2::PartialMatch(line, *pattern)) {
      return true;
    }
  }
  return false;
}
