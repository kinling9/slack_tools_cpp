#include "utils/sparse_graph_shortest_path_rf.h"

void sparse_graph_shortest_path_rf::build_graph(
    const std::vector<std::shared_ptr<Arc>> &edges) {
  build_graph_base(edges, [this](const std::shared_ptr<Arc> &edge) {
    if (_is_topin_rise) {
      return edge->delay[0];  // rise delay
    } else {
      return edge->delay[1];  // fall delay
    }
  });
}
