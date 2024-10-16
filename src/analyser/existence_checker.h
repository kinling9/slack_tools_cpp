#pragma once
#include <absl/container/flat_hash_map.h>
#include <re2/re2.h>

#include "analyser/analyser.h"
#include "dm/dm.h"

class existence_checker : public analyser {
 public:
  existence_checker(const YAML::Node &configs) : analyser(configs) {}
  ~existence_checker() override = default;
  void analyse() override {};
  void check_existence(
      const absl::flat_hash_map<std::string, std::string> &cell_maps,
      const std::shared_ptr<basedb> &db) {};

  absl::flat_hash_set<std::string> check_valid(YAML::Node &rpts) override {
    return {};
  }
  bool parse_configs() override { return true; }

 private:
  const RE2 _pin_pattern{"(.*)/[^/]*"};
};
