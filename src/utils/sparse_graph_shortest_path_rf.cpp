#include "utils/sparse_graph_shortest_path_rf.h"

#include <fmt/core.h>

#include <thread>

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
    // fmt::print("Rise Fall: {}, To pin: {}, Target delay: {}\n", _is_topin_rise,
    //            edge->to_pin, target_delay);
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
  {
    scoped_timer timer(timing_stats, "topo_sort");

    const int num_threads = 8;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    // Calculate work distribution
    const int work_per_thread = _components_computed / num_threads;
    const int remainder = _components_computed % num_threads;

    int start_idx = 0;
    for (int t = 0; t < num_threads; ++t) {
      // Distribute remainder work among first few threads
      int end_idx = start_idx + work_per_thread + (t < remainder ? 1 : 0);

      // Launch thread for this work chunk
      threads.emplace_back([start_idx, end_idx, this]() {
        for (int i = start_idx; i < end_idx; ++i) {
          // fmt::print("start comp {}, total comp {}\n", i, components_computed);
          topological_sort(i);
        }
      });

      start_idx = end_idx;
    }

    // Wait for all threads to complete
    for (auto &thread : threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }
}
