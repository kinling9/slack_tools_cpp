#pragma once
#include "arc_analyser.h"
#include "utils/sparse_graph_shortest_path_rf.h"
#include "yyjson.h"

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

  yyjson_mut_val *create_pin_node(
      yyjson_mut_doc *doc, const std::string &name, const bool is_input,
      const std::array<double, 2> &incr_delays,
      const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db,
      const bool is_topin_rise) const;

  void process_arc_segment(
      int t, size_t begin_idx, size_t end_idx,
      const std::vector<std::shared_ptr<Arc>> &arcs,
      const std::vector<std::string> &rpt_pair,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_key,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_value,
      std::vector<std::vector<std::pair<std::string, std::string>>>
          &thread_buffers,
      const std::shared_ptr<sparse_graph_shortest_path_rf> &graph_ptr,
      bool is_topin_rise);

 private:
  void process_single_connection(
      int t, const std::shared_ptr<Arc> &arc, const cache_result &connect_check,
      const std::vector<std::string> &rpt_pair,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_key,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_value,
      const std::tuple<std::string, bool, std::string, bool> &arc_tuple,
      std::vector<std::vector<std::pair<std::string, std::string>>>
          &thread_buffers,
      yyjson_mut_doc *doc);

 private:
  absl::flat_hash_map<std::string,
                      std::pair<std::shared_ptr<sparse_graph_shortest_path_rf>,
                                std::shared_ptr<sparse_graph_shortest_path_rf>>>
      _sparse_graph_ptrs;
};
