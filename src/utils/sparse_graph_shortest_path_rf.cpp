#include "utils/sparse_graph_shortest_path_rf.h"

#include "utils/scoped_timer.h"

void sparse_graph_shortest_path_rf::build_graph(
    const std::vector<std::shared_ptr<Arc>> &edges) {
  for (const auto &edge : edges) {
    int from_id = get_or_create_node_id(edge->from_pin);
    int to_id = get_or_create_node_id(edge->to_pin);
    double target_delay = 0.;
    if (_is_topin_rise) {
      target_delay = edge->delay[0];  // rise delay
    } else {
      target_delay = edge->delay[1];  // fall delay
    }
    _adj_list[from_id].emplace_back(to_id, target_delay);
    _rev_adj_list[to_id].emplace_back(from_id, target_delay);
    _all_nodes.insert(from_id);
    _all_nodes.insert(to_id);
  }
  _component_id.resize(_all_nodes.size(), 0);
  {
    scoped_timer timer(timing_stats, "compute_components_check");
    compute_components();
  }
}
