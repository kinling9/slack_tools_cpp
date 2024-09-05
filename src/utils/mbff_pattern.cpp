#include <absl/strings/match.h>
#include <fmt/core.h>

#include "mbff_pattern.h"
#include "yaml-cpp/yaml.h"

void mbff_pattern::load_pattern(const std::string& pattern_yml) {
  YAML::Node mbff_pattern;
  try {
    mbff_pattern = YAML::LoadFile(pattern_yml);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::exit(1);
  }
  fmt::print("MBFF match enabled, loading MBFF pattern from {}\n", pattern_yml);
  for (const auto& tool_pattern : mbff_pattern) {
    for (const auto& pattern : tool_pattern.second) {
      if (absl::StrContains(pattern.first.as<std::string>(), "merge")) {
        std::unique_ptr<RE2> pattern_merge =
            std::make_unique<RE2>(pattern.second.as<std::string>());
        _merge_pattern[tool_pattern.first.as<std::string>()] =
            std::move(pattern_merge);
      } else {
        std::unique_ptr<RE2> pattern_split =
            std::make_unique<RE2>(pattern.second.as<std::string>());
        _split_pattern[tool_pattern.first.as<std::string>()] =
            std::move(pattern_split);
      }
    }
  }
  _enable_mbff = true;
}

std::vector<std::string> mbff_pattern::get_ff_names(
    const std::string& tool, const std::string& line) const {
  if (!_enable_mbff) {
    return {line};
  }
  std::vector<std::string> ff_names;
  std::string start, end, ff_name0, ff_name1;
  if (RE2::FullMatch(line, *_merge_pattern.at(tool), &start, &ff_name0,
                     &ff_name1, &end)) {
    ff_names.push_back(fmt::format("{}/{}/{}", start, ff_name0, end));
    ff_names.push_back(fmt::format("{}/{}/{}", start, ff_name1, end));
  } else if (RE2::FullMatch(line, *_split_pattern.at(tool), &start, &ff_name0,
                            &end)) {
    ff_names.push_back(fmt::format("{}/{}/{}", start, ff_name0, end));
  } else {
    ff_names.push_back(line);
  }
  return ff_names;
}
