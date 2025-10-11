#pragma once
#include <absl/strings/match.h>

#include <unordered_map>
#include <vector>

#include "pair_analyser_csv.h"
#include "utils/sparse_graph_shortest_path.h"

class pair_analyser_dij : public pair_analyser_csv {
 public:
  pair_analyser_dij(const YAML::Node &configs) : pair_analyser_csv(configs){};
  void analyse() override;
  void init_graph(const std::shared_ptr<basedb> &db, std::string name);
  void csv_match(const std::vector<std::string> &rpt_pair,
                 absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>,
                                                std::shared_ptr<Arc>>> &arcs,
                 const std::unordered_map<std::string, std::shared_ptr<Pin>>
                     &csv_pin_db_key,
                 const std::unordered_map<std::string, std::shared_ptr<Pin>>
                     &csv_pin_db_value);

  nlohmann::json create_pin_node(
      const std::string &name, bool is_input, double incr_delay,
      const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db);

  void process_arc_segment(
      int t, size_t begin_idx, size_t end_idx,
      const absl::flat_hash_set<
          std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>> &arcs,
      const std::vector<std::string> &rpt_pair,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_key,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_value,
      std::vector<std::map<std::tuple<std::string, bool, std::string, bool>,
                           nlohmann::json>> &thread_buffers);
  absl::flat_hash_map<std::string, std::shared_ptr<sparse_graph_shortest_path>>
      _sparse_graph_ptrs;
};
