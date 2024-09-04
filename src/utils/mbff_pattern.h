#pragma once
#include <absl/container/flat_hash_map.h>
#include <re2/re2.h>

#include <memory>
#include <string>
#include <vector>

class mbff_pattern {
 public:
  mbff_pattern(const std::string &pattern_yml) : _pattern_yml(pattern_yml) {
    load_pattern();
  }
  void load_pattern();
  std::vector<std::string> get_ff_names(const std::string &tool,
                                        const std::string &line) const;

 private:
  std::string _pattern_yml;
  absl::flat_hash_map<std::string, std::unique_ptr<RE2>> _merge_pattern;
  absl::flat_hash_map<std::string, std::unique_ptr<RE2>> _split_pattern;
};
