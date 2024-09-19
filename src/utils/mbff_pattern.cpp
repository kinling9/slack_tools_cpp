#include "mbff_pattern.h"

#include <absl/strings/match.h>
#include <fmt/core.h>

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>

#include "yaml-cpp/yaml.h"

void mbff_pattern::load_pattern(const std::string &pattern_yml) {
  YAML::Node mbff_pattern;
  try {
    mbff_pattern = YAML::LoadFile(pattern_yml);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::exit(1);
  }
  for (const auto &tool_pattern : mbff_pattern) {
    for (const auto &pattern : tool_pattern.second) {
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
  fmt::print("MBFF match enabled, finsh loading MBFF pattern from {}\n",
             pattern_yml);
  _enable_mbff = true;
}

std::vector<std::string> mbff_pattern::get_ff_names(
    const std::string &tool, const std::string &line) const {
  if (!_enable_mbff) {
    return {line};
  }
  if (!_merge_pattern.contains(tool)) {
    return {line};
  }
  std::vector<std::string> ff_names;
  std::string start, end, ff_name0, ff_name1, num;
  if (RE2::PartialMatch(line, *_merge_pattern.at(tool), &start, &ff_name0,
                        &ff_name1, &end, &num)) {
    int iter = boost::convert<int>(num, boost::cnv::strtol()).value();
    if (iter == 1) {
      ff_names.push_back(fmt::format("{}/{}/{}", start, ff_name0, end));
    } else {
      ff_names.push_back(fmt::format("{}/{}/{}", start, ff_name1, end));
    }
  } else if (RE2::PartialMatch(line, *_split_pattern.at(tool), &start,
                               &ff_name0, &num, &end)) {
    int iter = boost::convert<int>(num, boost::cnv::strtol()).value();
    ff_names.push_back(
        fmt::format("{}/{}/{}{}", start, ff_name0, end, iter + 1));
    ff_names.push_back(line);
  } else {
    ff_names.push_back(line);
  }
  return ff_names;
}
