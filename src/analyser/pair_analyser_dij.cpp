#include "pair_analyser_dij.h"

#include <fmt/ranges.h>

#include <algorithm>
#include <limits>
#include <syncstream>
#include <thread>

#include "utils/utils.h"

SparseGraphShortestPath::SparseGraphShortestPath(
    const std::vector<std::shared_ptr<Arc>> &edges) {
  buildGraph(edges);
}

int SparseGraphShortestPath::getOrCreateNodeId(
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

std::string_view SparseGraphShortestPath::getNodeName(int node_id) const {
  if (node_id >= 0 && node_id < static_cast<int>(int_to_string.size())) {
    return int_to_string[node_id];
  }
  return "";
}

int SparseGraphShortestPath::getNodeId(
    const std::string_view &node_name) const {
  auto it = string_to_int.find(node_name);
  return (it != string_to_int.end()) ? it->second : -1;
}

void SparseGraphShortestPath::buildGraph(
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
    ScopedTimer timer(timing_stats, "compute_components_check");
    computeComponents();
  }
  {
    ScopedTimer timer(timing_stats, "topo_sort");

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

void SparseGraphShortestPath::computeComponents() {
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
  std::unique_lock lock(precomp_mutex);
  // distance_matrixs.resize(components_computed);
  // predecessor_matrixs.resize(components_computed);
  graph_sizes.reserve(components_computed);
  topological_orders.resize(components_computed);
  for (const auto &nodes : graph_components) {
    graph_sizes.push_back(nodes.size());
  }
  for (int i = 0; i < components_computed; ++i) {
    topological_orders[i].reserve(graph_sizes[i]);
  }
  lock.unlock();
}

void SparseGraphShortestPath::topologicalSort(int comp_id) {
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

CacheResult SparseGraphShortestPath::queryShortestDistance(
    const std::string_view &from, const std::string_view &to) {
  int from_id = getNodeId(from);
  int to_id = getNodeId(to);
  if (from_id == -1 || to_id == -1) {
    return {-1, {}};
  }
  return queryShortestDistanceById(from_id, to_id);
}

CacheResult SparseGraphShortestPath::queryShortestDistanceById(int from_id,
                                                               int to_id) {
  CacheResult result;
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
    ScopedTimer timer(timing_stats, "check_connectivity");
    if (component_id.at(from_id) != component_id.at(to_id)) {
      fmt::print(stderr,
                 "Debug: from_id {} and to_id {} are in different components\n",
                 from_id, to_id);
      return {-1, {}};
    }
  }
  {
    ScopedTimer timer(timing_stats, "dijkstra_call");
    result = dijkstra_topo(from_id, to_id, component_id.at(from_id));
  }
  return result;
}

CacheResult SparseGraphShortestPath::dijkstra_topo(int from_id, int to_id,
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

  return CacheResult(-1, {});  // 不可达
}

CacheResult SparseGraphShortestPath::reconstruct_path(
    int from_id, int to_id, const std::unordered_map<int, int> &previous,
    double distance) const {
  CacheResult cache_result(distance, {});
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

void SparseGraphShortestPath::printStats() const {
  int edge_count = 0;
  for (const auto &[node_id, neighbors] : adj_list) {
    edge_count += neighbors.size();
  }
  fmt::print("Number of nodes: {}\n", all_nodes.size());
  fmt::print("Number of edges: {}\n", edge_count);
  fmt::print("String to int mappings: {}\n", string_to_int.size());
}

void pair_analyser_dij::analyse() {
  // if (_enable_rise_fall) {
  //   fmt::print("Enable rise fall check\n");
  //   _rf_checker.set_enable_rise_fall(true);
  // }
  open_writers();
  fmt::print("Analyse tuples: {}\n", fmt::join(_analyse_tuples, ", "));
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        arcs;
    gen_arc_tuples(_dbs.at(rpt_pair[0]), arcs);
    init_graph(_dbs.at(rpt_pair[1]), rpt_pair[1]);
    csv_match(rpt_pair, arcs, _dbs.at(rpt_pair[0])->pins,
              _dbs.at(rpt_pair[1])->pins);
  }
}

void pair_analyser_dij::init_graph(const std::shared_ptr<basedb> &db,
                                   std::string name) {
  if (db == nullptr) {
    fmt::print("DB is nullptr, skip\n");
    return;
  }
  auto graph = std::make_shared<SparseGraphShortestPath>(db->all_arcs);
  _sparse_graph_ptrs[name] = graph;
  graph->printStats();
}

nlohmann::json pair_analyser_dij::create_pin_node(
    const std::string &name, bool is_input, double incr_delay,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db) {
  nlohmann::json node;
  node["name"] = name;
  node["is_input"] = is_input;
  node["incr_delay"] = incr_delay;
  node["rf"] = false;
  if (!csv_pin_db.empty()) {
    if (auto pin_it = csv_pin_db.find(name); pin_it != csv_pin_db.end()) {
      const auto &pin = pin_it->second;
      node["path_delay"] = pin->path_delay;
      node["location"] = {pin->location.first, pin->location.second};
      node["trans"] = pin->trans;
      node["cap"] = pin->cap.value_or(0.);
    }
  }
  return node;
}

void pair_analyser_dij::process_arc_segment(
    int t, size_t begin_idx, size_t end_idx,
    const absl::flat_hash_set<
        std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>> &arcs,
    const std::vector<std::string> &rpt_pair,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value,
    std::vector<std::map<std::tuple<std::string, bool, std::string, bool>,
                         nlohmann::json>> &thread_buffers) {
  if (!_sparse_graph_ptrs.contains(rpt_pair[1])) {
    fmt::print("No graph for type {}\n", rpt_pair[1]);
    return;
  }

  auto it = arcs.begin();
  std::advance(it, begin_idx);
  auto end_it = arcs.begin();
  std::advance(end_it, end_idx);

  for (; it != end_it; ++it) {
    const auto &[arc_cell, arc_net] = *it;
    const auto &pin_from = arc_cell->from_pin;
    const auto &pin_inter = arc_cell->to_pin;
    const auto &pin_to = arc_net->to_pin;
    auto from = std::make_pair(pin_from, false);
    auto to = std::make_pair(pin_to, false);
    auto arc_tuple = std::make_tuple(pin_from, false, pin_to, false);

    auto connect_check = _sparse_graph_ptrs[rpt_pair[1]]->queryShortestDistance(
        pin_from, pin_to);
    if (connect_check.distance < 0) {
      fmt::print("No connection from {} to {}, skip\n", pin_from, pin_to);
      continue;
    }

    auto max_cell_delay = std::max(arc_cell->delay[0], arc_cell->delay[1]);
    auto max_net_delay = std::max(arc_net->delay[0], arc_net->delay[1]);

    nlohmann::json node;
    node["type"] = "pair arc";
    node["from"] =
        fmt::format("{} {}", from.first, from.second ? "(rise)" : "(fall)");
    node["to"] =
        fmt::format("{} {}", to.first, to.second ? "(rise)" : "(fall)");

    // Pre-calculate delay
    const double total_delay = max_cell_delay + max_net_delay;

    // Build key section
    nlohmann::json key_pins = nlohmann::json::array();
    key_pins.push_back(create_pin_node(pin_from, true, 0, csv_pin_db_key));
    key_pins.push_back(
        create_pin_node(pin_inter, false, max_cell_delay, csv_pin_db_key));
    key_pins.push_back(
        create_pin_node(pin_to, true, max_net_delay, csv_pin_db_key));

    node["key"] = {{"pins", std::move(key_pins)}, {"delay", total_delay}};

    node["value"] = {{"pins", nlohmann::json::array()},
                     {"delay", connect_check.distance}};

    node["value"]["pins"].push_back(
        create_pin_node(pin_from, true, 0, csv_pin_db_value));

    bool is_cell_arc = true;
    auto value_db = _dbs.at(rpt_pair[1]);
    for (const auto &pin_tuple : connect_check.path | std::views::adjacent<2>) {
      const auto &[mid_from_view, mid_to_view] = pin_tuple;
      std::string mid_from = std::string(mid_from_view);
      std::string mid_to = std::string(mid_to_view);
      std::shared_ptr<Arc> &mid_arc =
          is_cell_arc ? value_db->cell_arcs[mid_from][mid_to]
                      : value_db->net_arcs[mid_from][mid_to];
      is_cell_arc = !is_cell_arc;
      node["value"]["pins"].push_back(create_pin_node(
          mid_to, !is_cell_arc, mid_arc->delay[0], csv_pin_db_value));
    }

    if (arc_net->fanout.has_value()) {
      node["key"]["fanout"] = arc_net->fanout.value();
    }

    bool valid_location = true;
    if (csv_pin_db_key.contains(pin_inter)) {
      node["key"]["slack"] =
          csv_pin_db_key.at(pin_inter)->path_slack.value_or(0.0);
      node["value"]["slack"] =
          csv_pin_db_value.contains(pin_inter)
              ? csv_pin_db_value.at(pin_inter)->path_slack.value_or(0.0)
              : 0.0;
      node["delta_slack"] = node["key"]["slack"].get<double>() -
                            node["value"]["slack"].get<double>();
    }

    std::vector<std::pair<float, float>> locs_key;
    std::vector<std::pair<float, float>> locs_value;

    if (!csv_pin_db_key.empty()) {
      for (const auto &pin : node["key"]["pins"]) {
        if (!pin.contains("location")) {
          valid_location = false;
          break;
        }
        locs_key.emplace_back(pin["location"][0].get<double>(),
                              pin["location"][1].get<double>());
      }
    }

    if (valid_location && !csv_pin_db_value.empty()) {
      for (const auto &pin : node["value"]["pins"]) {
        if (!pin.contains("location")) {
          valid_location = false;
          break;
        }
        locs_value.emplace_back(pin["location"][0].get<double>(),
                                pin["location"][1].get<double>());
      }
    }

    if (valid_location) {
      node["key"]["length"] = manhattan_distance(locs_key);
      node["value"]["length"] = manhattan_distance(locs_value);
      node["delta_length"] = node["key"]["length"].get<double>() -
                             node["value"]["length"].get<double>();
    }

    double delta_delay = node["key"]["delay"].get<double>() -
                         node["value"]["delay"].get<double>();
    node["delta_delay"] = delta_delay;

    // Store in thread-local buffer
    thread_buffers[t][arc_tuple] = std::move(node);
  }
}

void pair_analyser_dij::csv_match(
    const std::vector<std::string> &rpt_pair,
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        &arcs,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value) {
  absl::flat_hash_map<std::tuple<std::string, bool, std::string, bool>,
                      nlohmann::json>
      arcs_buffer;

  unsigned int num_threads =
      std::max(1u, std::min(8u, static_cast<unsigned int>(arcs.size())));
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  std::unordered_set<std::string_view> arc_starts;
  for (const auto &[arc_cell, arc_net] : arcs) {
    arc_starts.insert(arc_cell->from_pin);
  }

  std::vector<std::map<std::tuple<std::string, bool, std::string, bool>,
                       nlohmann::json>>
      thread_buffers(num_threads);
  size_t chunk_size_arc = (arcs.size() + num_threads - 1) / num_threads;

  for (unsigned int t = 0; t < num_threads; ++t) {
    size_t begin_idx = t * chunk_size_arc;
    size_t end_idx = std::min(begin_idx + chunk_size_arc, arcs.size());

    if (begin_idx >= arcs.size()) break;

    threads.emplace_back(&pair_analyser_dij::process_arc_segment, this, t,
                         begin_idx, end_idx, std::ref(arcs), std::ref(rpt_pair),
                         std::ref(csv_pin_db_key), std::ref(csv_pin_db_value),
                         std::ref(thread_buffers));
  }

  // Wait for all threads
  for (auto &th : threads) {
    if (th.joinable()) {
      th.join();
    }
  }

  // process_arc_segment(0, 0, arcs.size(), arcs, rpt_pair, csv_pin_db_key,
  //                     csv_pin_db_value, thread_buffers);

  // Merge results
  for (auto &buffer : thread_buffers) {
    arcs_buffer.insert(buffer.begin(), buffer.end());
  }
  for (auto [name, time] : _sparse_graph_ptrs[rpt_pair[1]]->timing_stats) {
    fmt::print("Timing stats {}: {} s\n", name, time / 1e6);
  }
  nlohmann::json arc_node;
  for (const auto &[arc, _] : arcs_buffer) {
    arc_node[fmt::format(
        "{} {}-{} {}", std::get<0>(arc), std::get<1>(arc) ? "(rise)" : "(fall)",
        std::get<2>(arc), std::get<3>(arc) ? "(rise)" : "(fall)")] =
        arcs_buffer[arc];
  }

  std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
  fmt::print(_arcs_writers[cmp_name]->out_file, "{}", arc_node.dump(2));
  fmt::print("Wrote {} arc pairs to {}\n", arc_node.size(), cmp_name);
}
