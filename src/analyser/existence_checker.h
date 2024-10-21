#pragma once
#include <absl/container/flat_hash_map.h>

#include "analyser/analyser.h"
#include "dm/dm.h"

class existence_checker : public analyser {
 public:
  existence_checker(const YAML::Node &configs) : analyser(configs, 1) {}
  ~existence_checker() override = default;
  void analyse() override;
  void check_existence(const std::shared_ptr<basedb> &db,
                       const std::string &key);
};
