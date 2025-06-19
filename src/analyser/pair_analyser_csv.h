#pragma once
#include "arc_analyser.h"

class pair_analyser_csv : public arc_analyser {
 public:
  pair_analyser_csv(const YAML::Node &configs) : arc_analyser(configs){};
  void analyse() override;
  void csv_match(const std::string &cmp_name,
                 absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>,
                                                std::shared_ptr<Arc>>> &arcs,
                 absl::flat_hash_map<std::pair<std::string_view, bool>,
                                     std::shared_ptr<Path>> &pin_map,
                 const std::vector<std::shared_ptr<basedb>> &dbs);
  void gen_arc_tuples(
      const std::shared_ptr<basedb> &db,
      absl::flat_hash_set<
          std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>> &arcs);
};
