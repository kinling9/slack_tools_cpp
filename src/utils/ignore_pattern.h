#pragma once
#include <re2/re2.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ignore_pattern {
 public:
  ignore_pattern(const std::string &pattern_yml) : _pattern_yml(pattern_yml) {
    load_pattern(pattern_yml);
  }
  ignore_pattern() {}
  void load_pattern(const std::string &pattern_yml);
  bool check_ignore(const std::string &tool, const std::string &line) const;

 private:
  bool _enable_ignore = false;
  std::string _pattern_yml;
  std::unordered_map<std::string, std::unique_ptr<RE2>> _patterns;
};
