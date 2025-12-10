#include "utils/sparse_graph_shortest_path.h"

#include <fmt/core.h>

#include <queue>
#include <thread>

#include "utils/scoped_timer.h"

int sparse_graph_shortest_path::get_or_create_node_id(
    const std::string_view &node_name) {
  auto it = _string_to_int.find(node_name);
  if (it != _string_to_int.end()) {
    return it->second;
  }
  int node_id = _next_node_id++;
  _string_to_int[node_name] = node_id;
  _int_to_string.push_back(node_name);
  return node_id;
}

std::string_view sparse_graph_shortest_path::get_node_name(int node_id) const {
  if (node_id >= 0 && node_id < static_cast<int>(_int_to_string.size())) {
    return _int_to_string[node_id];
  }
  return "";
}

int sparse_graph_shortest_path::get_node_id(
    const std::string_view &node_name) const {
  auto it = _string_to_int.find(node_name);
  return (it != _string_to_int.end()) ? it->second : -1;
}

void sparse_graph_shortest_path::build_graph(
    const std::vector<std::shared_ptr<Arc>> &edges) {
  build_graph_base(edges, [](const std::shared_ptr<Arc> &edge) {
    if constexpr (dm::TARGET_DLY_USING_MAX) {
      return std::max(edge->delay[0], edge->delay[1]);
    } else {
      return std::min(edge->delay[0], edge->delay[1]);
    }
  });
}

void sparse_graph_shortest_path::build_graph_base(
    const std::vector<std::shared_ptr<Arc>> &edges,
    std::function<double(const std::shared_ptr<Arc> &)> delay_extractor) {
  for (const auto &edge : edges) {
    int from_id = get_or_create_node_id(edge->from_pin);
    int to_id = get_or_create_node_id(edge->to_pin);
    double target_delay = delay_extractor(edge);
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

void sparse_graph_shortest_path::compute_components() {
  if (_components_computed != 0) return;
  std::unordered_set<int> visited;
  int comp_id = 0;
  for (int node_id : _all_nodes) {
    if (visited.find(node_id) == visited.end()) {
      _graph_components.resize(comp_id + 1);
      std::queue<int> q;
      q.push(node_id);
      visited.insert(node_id);
      _component_id[node_id] = comp_id;
      _graph_components[comp_id].push_back(node_id);
      while (!q.empty()) {
        int curr = q.front();
        q.pop();
        if (_adj_list.find(curr) != _adj_list.end()) {
          for (auto &[neighbor_id, _] : _adj_list[curr]) {
            if (visited.find(neighbor_id) == visited.end()) {
              visited.insert(neighbor_id);
              _component_id[neighbor_id] = comp_id;
              _graph_components[comp_id].push_back(neighbor_id);
              q.push(neighbor_id);
            }
          }
        }
        if (_rev_adj_list.find(curr) != _rev_adj_list.end()) {
          for (auto &[neighbor_id, _] : _rev_adj_list[curr]) {
            if (visited.find(neighbor_id) == visited.end()) {
              visited.insert(neighbor_id);
              _component_id[neighbor_id] = comp_id;
              _graph_components[comp_id].push_back(neighbor_id);
              q.push(neighbor_id);
            }
          }
        }
      }
      comp_id++;
    }
  }
  _components_computed = comp_id;
  _graph_sizes.reserve(_components_computed);
  _topological_orders.resize(_components_computed);
  for (const auto &nodes : _graph_components) {
    _graph_sizes.push_back(nodes.size());
  }
  for (int i = 0; i < _components_computed; ++i) {
    _topological_orders[i].reserve(_graph_sizes[i]);
  }
}

void sparse_graph_shortest_path::topological_sort(int comp_id) {
  std::unique_lock lock(_component_mutex);
  const auto nodes = std::move(_graph_components[comp_id]);
  lock.unlock();
  const size_t node_count = nodes.size();

  // Early exit for trivial cases
  if (node_count <= 1) {
    return;
  }

  // Otherwise, reserve space for better performance
  std::unordered_map<int, int> in_degree;
  in_degree.reserve(node_count);

  // Initialize in-degrees
  for (int node : nodes) {
    in_degree[node] = 0;
  }

  // Calculate in-degrees
  for (int node : nodes) {
    auto it = _adj_list.find(node);
    if (it != _adj_list.end()) {
      for (const auto &neighbor : it->second) {
        in_degree[neighbor.first]++;
      }
    }
  }

  // Use vector as queue for better cache performance
  std::vector<int> queue;
  queue.reserve(node_count);
  std::vector<int> result;
  result.reserve(node_count);

  // Find all nodes with in-degree 0
  for (const auto &[node, degree] : in_degree) {
    if (degree == 0) {
      queue.push_back(node);
    }
  }

  size_t queue_pos = 0;

  // Process nodes
  while (queue_pos < queue.size()) {
    int u = queue[queue_pos++];
    result.push_back(u);

    auto it = _adj_list.find(u);
    if (it != _adj_list.end()) {
      for (const auto &neighbor : it->second) {
        int v = neighbor.first;
        if (--in_degree[v] == 0) {
          queue.push_back(v);
        }
      }
    }
  }

  // Update the component with sorted result
  lock.lock();
  _graph_components[comp_id] = std::move(result);
  for (size_t i = 0; i < _graph_components[comp_id].size(); ++i) {
    int node = _graph_components[comp_id][i];
    _topological_orders[comp_id][node] = i;
  }
}

cache_result sparse_graph_shortest_path::query_shortest_distance(
    const std::string_view &from, const std::string_view &to) {
  int from_id = get_node_id(from);
  int to_id = get_node_id(to);
  if (from_id == -1 || to_id == -1) {
    return {-1, {}};
  }
  return query_shortest_distance_by_id(from_id, to_id);
}

cache_result sparse_graph_shortest_path::query_shortest_distance_by_id(
    int from_id, int to_id) {
  cache_result result;
  if (_all_nodes.find(from_id) == _all_nodes.end()) {
    fmt::print(stderr, "Debug: from_id {} does not exist in all_nodes\n",
               from_id);
    return {-1, {}};
  }
  if (_all_nodes.find(to_id) == _all_nodes.end()) {
    fmt::print(stderr, "Debug: to_id {} does not exist in all_nodes\n", to_id);
    return {-1, {}};
  }
  if (from_id == to_id) {
    return {0, {get_node_name(from_id)}};
  }
  {
    // scoped_timer timer(timing_stats, "check_connectivity");
    if (_component_id.at(from_id) != _component_id.at(to_id)) {
      fmt::print(stderr,
                 "Debug: from_id {} and to_id {} are in different components\n",
                 from_id, to_id);
      return {-1, {}};
    }
  }
  {
    // scoped_timer timer(timing_stats, "dijkstra_call");
    result = dijkstra_topo(from_id, to_id, _component_id.at(from_id));
  }
  return result;
}

cache_result sparse_graph_shortest_path::dijkstra_topo(int from_id, int to_id,
                                                       int comp_id) const {
  using namespace std;
  // 优先队列：pair<distance, node>
  priority_queue<pair<double, int>, vector<pair<double, int>>,
                 greater<pair<double, int>>>
      pq;

  // int n_nodes = graph_sizes.at(comp_id);
  unordered_map<int, double> dist;
  unordered_map<int, int> parent;
  unordered_map<int, bool> visited;

  constexpr int bucket_size = 64;
  parent.reserve(bucket_size);
  dist.reserve(bucket_size);
  visited.reserve(bucket_size);

  dist[to_id] = 0.0;
  pq.push({0.0, to_id});

  int from_id_pos = _topological_orders.at(comp_id).at(from_id);

  while (!pq.empty()) {
    auto [d, u] = pq.top();
    pq.pop();

    if (visited.find(u) != visited.end() || visited[u]) continue;
    visited[u] = true;

    // 找到目标
    if (u == from_id) {
      return reconstruct_path(from_id, to_id, parent, d);
    }

    // 拓扑序剪枝：只扩展位置在from_id之后的节点
    if (_topological_orders.at(comp_id).at(u) < from_id_pos) continue;

    // 遍历出边
    if (_rev_adj_list.find(u) != _rev_adj_list.end()) {
      for (const auto &[v, w] : _rev_adj_list.at(u)) {
        // 只考虑拓扑序小于等于to_id的节点
        if (_topological_orders.at(comp_id).at(v) < from_id_pos) continue;

        double new_dist = d + w;
        if (dist.find(v) == dist.end() || new_dist < dist[v]) {
          dist[v] = new_dist;
          parent[v] = u;
          pq.push({new_dist, v});
        }
      }
    }
  }

  return cache_result(-1, {});  // 不可达
}

cache_result sparse_graph_shortest_path::reconstruct_path(
    int from_id, int to_id, const std::unordered_map<int, int> &previous,
    double distance) const {
  cache_result cache_result(distance, {});
  if (to_id == from_id) {
    cache_result.path = {get_node_name(from_id)};
    return cache_result;
  }
  std::vector<std::string_view> path;
  int current = from_id;
  while (current != to_id && previous.find(current) != previous.end()) {
    path.push_back(get_node_name(current));
    current = previous.at(current);
  }
  if (current == to_id) {
    path.push_back(get_node_name(to_id));
    // std::reverse(path.begin(), path.end());
    cache_result.path = path;
  }
  return cache_result;
}

void sparse_graph_shortest_path::print_stats() const {
  int edge_count = 0;
  for (const auto &[node_id, neighbors] : _adj_list) {
    edge_count += neighbors.size();
  }
  fmt::print("Number of nodes: {}\n", _all_nodes.size());
  fmt::print("Number of edges: {}\n", edge_count);
  fmt::print("String to int mappings: {}\n", _string_to_int.size());
}
