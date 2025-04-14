#pragma once
#include "arc_analyser.h"

class pair_analyser : public arc_analyser {
 public:
  pair_analyser(const YAML::Node &configs) : arc_analyser(configs){};
  void match(const std::string &cmp_name,
             const absl::flat_hash_map<std::pair<std::string_view, bool>,
                                       std::shared_ptr<Path>> &pin_map,
             const std::vector<std::shared_ptr<basedb>> &dbs) override;
};
