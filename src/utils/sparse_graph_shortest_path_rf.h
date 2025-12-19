#pragma once
#include "utils/sparse_graph_shortest_path.h"

class sparse_graph_shortest_path_rf : public sparse_graph_shortest_path {
 public:
  sparse_graph_shortest_path_rf(bool is_topin_rise)
      : sparse_graph_shortest_path(), _is_topin_rise(is_topin_rise) {}

  void build_graph(const std::vector<std::shared_ptr<Arc>> &edges) override;

 private:
  bool _is_topin_rise;
};
