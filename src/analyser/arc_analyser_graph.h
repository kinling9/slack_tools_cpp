#pragma once
#include "arc_analyser.h"
#include "utils/sparse_graph_shortest_path.h"
class arc_analyser_graph : public arc_analyser {
 public:
  arc_analyser_graph(const YAML::Node &configs) : arc_analyser(configs){};
  void analyse() override;
  void init_graph(const std::shared_ptr<basedb> &db, std::string name);

  void csv_match(const std::vector<std::string> &rpt_pair,
                 const std::unordered_map<std::string, std::shared_ptr<Pin>>
                     &csv_pin_db_key,
                 const std::unordered_map<std::string, std::shared_ptr<Pin>>
                     &csv_pin_db_value);

  nlohmann::json create_pin_node(
      const std::string &name, bool is_input,
      const std::array<double, 2> &incr_delays,
      const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db);

  void process_arc_segment(
      int t, size_t begin_idx, size_t end_idx,
      const std::vector<std::shared_ptr<Arc>> &arcs,
      const std::vector<std::string> &rpt_pair,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_key,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_value,
      std::vector<std::map<std::tuple<std::string, bool, std::string, bool>,
                           nlohmann::json>> &thread_buffers);

 private:
  absl::flat_hash_map<std::string, std::shared_ptr<sparse_graph_shortest_path>>
      _sparse_graph_ptrs;
};
