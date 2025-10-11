#include "utils/sparse_graph_shortest_path.h"

#include <fmt/core.h>

#include <queue>
#include <thread>

#include "utils/scoped_timer.h"

sparse_graph_shortest_path::sparse_graph_shortest_path(
    const std::vector<std::shared_ptr<Arc>> &edges) {
  buildGraph(edges);
}

int sparse_graph_shortest_path::getOrCreateNodeId(
    const std::string_view &node_name) {
  auto it = string_to_int.find(node_name);
  if (it != string_to_int.end()) {
    return it->second;
  }
  int node_id = next_node_id++;
  string_to_int[node_name] = node_id;
  int_to_string.push_back(node_name);
  return node_id;
}

std::string_view sparse_graph_shortest_path::getNodeName(int node_id) const {
  if (node_id >= 0 && node_id < static_cast<int>(int_to_string.size())) {
    return int_to_string[node_id];
  }
  return "";
}

int sparse_graph_shortest_path::getNodeId(
    const std::string_view &node_name) const {
  auto it = string_to_int.find(node_name);
  return (it != string_to_int.end()) ? it->second : -1;
}

void sparse_graph_shortest_path::buildGraph(
    const std::vector<std::shared_ptr<Arc>> &edges) {
  for (const auto &edge : edges) {
    int from_id = getOrCreateNodeId(edge->from_pin);
    int to_id = getOrCreateNodeId(edge->to_pin);
    double max_delay = std::max(edge->delay[0], edge->delay[1]);
    adj_list[from_id].emplace_back(to_id, max_delay);
    rev_adj_list[to_id].emplace_back(from_id, max_delay);
    all_nodes.insert(from_id);
    all_nodes.insert(to_id);
  }
  component_id.resize(all_nodes.size(), 0);
  {
    scoped_timer timer(timing_stats, "compute_components_check");
    computeComponents();
  }
  {
    scoped_timer timer(timing_stats, "topo_sort");

    const int num_threads = 8;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    // Calculate work distribution
    const int work_per_thread = components_computed / num_threads;
    const int remainder = components_computed % num_threads;

    int start_idx = 0;
    for (int t = 0; t < num_threads; ++t) {
      // Distribute remainder work among first few threads
      int end_idx = start_idx + work_per_thread + (t < remainder ? 1 : 0);

      // Launch thread for this work chunk
      threads.emplace_back([start_idx, end_idx, this]() {
        for (int i = start_idx; i < end_idx; ++i) {
          // fmt::print("start comp {}, total comp {}\n", i, components_computed);
          topologicalSort(i);
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

void sparse_graph_shortest_path::computeComponents() {
  if (components_computed != 0) return;
  std::unordered_set<int> visited;
  int comp_id = 0;
  for (int node_id : all_nodes) {
    if (visited.find(node_id) == visited.end()) {
      graph_components.resize(comp_id + 1);
      std::queue<int> q;
      q.push(node_id);
      visited.insert(node_id);
      component_id[node_id] = comp_id;
      graph_components[comp_id].push_back(node_id);
      while (!q.empty()) {
        int curr = q.front();
        q.pop();
        if (adj_list.find(curr) != adj_list.end()) {
          for (auto &[neighbor_id, _] : adj_list[curr]) {
            if (visited.find(neighbor_id) == visited.end()) {
              visited.insert(neighbor_id);
              component_id[neighbor_id] = comp_id;
              graph_components[comp_id].push_back(neighbor_id);
              q.push(neighbor_id);
            }
          }
        }
        if (rev_adj_list.find(curr) != adj_list.end()) {
          for (auto &[neighbor_id, _] : rev_adj_list[curr]) {
            if (visited.find(neighbor_id) == visited.end()) {
              visited.insert(neighbor_id);
              component_id[neighbor_id] = comp_id;
              graph_components[comp_id].push_back(neighbor_id);
              q.push(neighbor_id);
            }
          }
        }
      }
      comp_id++;
    }
  }
  components_computed = comp_id;
  graph_sizes.reserve(components_computed);
  topological_orders.resize(components_computed);
  for (const auto &nodes : graph_components) {
    graph_sizes.push_back(nodes.size());
  }
  for (int i = 0; i < components_computed; ++i) {
    topological_orders[i].reserve(graph_sizes[i]);
  }
}

void sparse_graph_shortest_path::topologicalSort(int comp_id) {
  std::unique_lock lock(component_mutex);
  const auto nodes = std::move(graph_components[comp_id]);
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
    auto it = adj_list.find(node);
    if (it != adj_list.end()) {
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

    auto it = adj_list.find(u);
    if (it != adj_list.end()) {
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
  graph_components[comp_id] = std::move(result);
  for (size_t i = 0; i < graph_components[comp_id].size(); ++i) {
    int node = graph_components[comp_id][i];
    topological_orders[comp_id][node] = i;
  }
}

cache_result sparse_graph_shortest_path::queryShortestDistance(
    const std::string_view &from, const std::string_view &to) {
  int from_id = getNodeId(from);
  int to_id = getNodeId(to);
  if (from_id == -1 || to_id == -1) {
    return {-1, {}};
  }
  return queryShortestDistanceById(from_id, to_id);
}

cache_result sparse_graph_shortest_path::queryShortestDistanceById(int from_id,
                                                                   int to_id) {
  cache_result result;
  if (all_nodes.find(from_id) == all_nodes.end()) {
    fmt::print(stderr, "Debug: from_id {} does not exist in all_nodes\n",
               from_id);
    return {-1, {}};
  }
  if (all_nodes.find(to_id) == all_nodes.end()) {
    fmt::print(stderr, "Debug: to_id {} does not exist in all_nodes\n", to_id);
    return {-1, {}};
  }
  if (from_id == to_id) {
    return {0, {getNodeName(from_id)}};
  }
  {
    scoped_timer timer(timing_stats, "check_connectivity");
    if (component_id.at(from_id) != component_id.at(to_id)) {
      fmt::print(stderr,
                 "Debug: from_id {} and to_id {} are in different components\n",
                 from_id, to_id);
      return {-1, {}};
    }
  }
  {
    scoped_timer timer(timing_stats, "dijkstra_call");
    result = dijkstra_topo(from_id, to_id, component_id.at(from_id));
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

  dist[from_id] = 0.0;
  pq.push({0.0, from_id});

  int to_id_pos = topological_orders.at(comp_id).at(to_id);

  while (!pq.empty()) {
    auto [d, u] = pq.top();
    pq.pop();

    if (visited.find(u) != visited.end() || visited[u]) continue;
    visited[u] = true;

    // 找到目标
    if (u == to_id) {
      return reconstruct_path(from_id, to_id, parent, d);
    }

    // 拓扑序剪枝：只扩展位置在to_id之前的节点
    if (topological_orders.at(comp_id).at(u) >= to_id_pos) continue;

    // 遍历出边
    if (adj_list.find(u) != adj_list.end()) {
      for (const auto &[v, w] : adj_list.at(u)) {
        // 只考虑拓扑序小于等于to_id的节点
        if (topological_orders.at(comp_id).at(v) > to_id_pos) continue;

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
    cache_result.path = {getNodeName(from_id)};
    return cache_result;
  }
  std::vector<std::string_view> path;
  int current = to_id;
  while (current != from_id && previous.find(current) != previous.end()) {
    path.push_back(getNodeName(current));
    current = previous.at(current);
  }
  if (current == from_id) {
    path.push_back(getNodeName(from_id));
    std::reverse(path.begin(), path.end());
    cache_result.path = path;
  }
  return cache_result;
}

void sparse_graph_shortest_path::printStats() const {
  int edge_count = 0;
  for (const auto &[node_id, neighbors] : adj_list) {
    edge_count += neighbors.size();
  }
  fmt::print("Number of nodes: {}\n", all_nodes.size());
  fmt::print("Number of edges: {}\n", edge_count);
  fmt::print("String to int mappings: {}\n", string_to_int.size());
}
