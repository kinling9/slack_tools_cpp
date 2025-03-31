#pragma once

#include <nlohmann/json.hpp>

#include "dm/dm.h"

namespace super_arc {

nlohmann::json to_json(const std::shared_ptr<Path> path,
                       std::tuple<std::string, bool, std::string, bool> names,
                       bool enable_rise_fall);

class super_arc_pattern {
 public:
  super_arc_pattern(const std::string &pattern_yml)
      : _pattern_yml(pattern_yml) {
    load_pattern(pattern_yml);
  }
  super_arc_pattern() {}
  void load_pattern(const std::string &pattern_yml);
  bool check_super_arc(const std::string &tool, const std::string &line) const;

 private:
  bool _enable_super_arc = false;
  std::string _pattern_yml;
  std::unordered_map<std::string, std::vector<std::unique_ptr<RE2>>> _patterns;
};

}  // namespace super_arc
