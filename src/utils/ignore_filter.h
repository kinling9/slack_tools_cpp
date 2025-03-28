#pragma once

#include <nlohmann/json.hpp>

#include "dm/dm.h"
#include "utils/utils.h"

namespace ignore_filter {

class ignore_filter_pattern {
 public:
  ignore_filter_pattern(const std::string &pattern_yml)
      : _pattern_yml(pattern_yml) {
    load_pattern(pattern_yml);
  }
  ignore_filter_pattern() {}
  void load_pattern(const std::string &pattern_yml);
  bool check_ignore_filter(const std::string &line) const;

 private:
  bool _enable_ignore_filter = false;
  std::string _pattern_yml;
  std::vector<std::unique_ptr<RE2>> _patterns;
};

}  // namespace ignore_filter
