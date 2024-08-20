#pragma once
#include <absl/container/flat_hash_map.h>
#include <re2/re2.h>

#include "analyser/analyser.h"
#include "dm/dm.h"

class existence_checker : public analyser {
 public:
  existence_checker(const configs &configs) : analyser(configs) {};
  void analyse() override {};
  void check_existence(
      const absl::flat_hash_map<std::string, std::string> &cell_maps,
      const std::shared_ptr<basedb> &db);

 private:
  const RE2 _pin_pattern{"(.*)/[^/]*"};
};
