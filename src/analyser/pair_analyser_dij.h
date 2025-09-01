#pragma once
#include <absl/strings/match.h>

#include <chrono>
#include <climits>
#include <functional>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pair_analyser_csv.h"
class ScopedTimer {
 public:
  using Clock = std::chrono::high_resolution_clock;

  ScopedTimer(std::map<std::string, long long> &accum, const std::string &name)
      : m_accum(accum), m_name(name), m_start(Clock::now()) {}

  ~ScopedTimer() {
    auto end = Clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - m_start)
            .count();
    m_accum[m_name] += duration;
  }

 private:
  std::map<std::string, long long> &m_accum;
  std::string m_name;
  std::chrono::time_point<Clock> m_start;
};

class CacheResult {
 public:
  double distance;
  std::vector<std::string_view> path;
  CacheResult(double dist, const std::vector<std::string_view> &p)
      : distance(dist), path(p) {}
  CacheResult() : distance(-1), path({}) {}
};

class SparseGraphShortestPath {
 private:
  // 邻接表存储图结构 (使用int作为节点ID)
  std::unordered_map<int, std::vector<std::pair<int, double>>>
      adj_list;  // {node_id: [(neighbor_id, dist)]}
  std::unordered_map<int, std::vector<std::pair<int, double>>>
      rev_adj_list;  // {node_id: [(neighbor_id, dist)]}

  // 缓存已计算的最短距离，使用两级哈希表
  std::unordered_map<int, std::unordered_map<int, CacheResult>> distance_cache;

  // 存储所有节点ID
  std::unordered_set<int> all_nodes;

  // 连通分量信息
  std::unordered_map<int, int> component_id;  // node_id -> component_id
  int components_computed = 0;
  bool topo_sorted = false;

  // String和int的双向映射
  std::unordered_map<std::string_view, int> string_to_int;  // string -> int
  std::vector<std::string_view> int_to_string;              // int -> string
  int next_node_id = 0;

 public:
  std::map<std::string, long long> timing_stats;
  std::vector<std::vector<int>> graph_components;
  std::vector<std::unordered_map<int, std::unordered_map<int, double>>>
      distance_matrixs;  // 距离矩阵
  std::vector<std::unordered_map<int, std::unordered_map<int, int>>>
      predecessor_matrixs;  // 前驱矩阵

 public:
  // 构造函数，输入边的向量
  SparseGraphShortestPath(const std::vector<std::shared_ptr<Arc>> &edges) {
    buildGraph(edges);
  }

  // 获取或创建节点的int映射
  int getOrCreateNodeId(const std::string_view &node_name) {
    auto it = string_to_int.find(node_name);
    if (it != string_to_int.end()) {
      return it->second;
    }

    // 创建新的映射
    int node_id = next_node_id++;
    string_to_int[node_name] = node_id;
    int_to_string.push_back(node_name);
    return node_id;
  }

  // 根据int获取string
  std::string_view getNodeName(int node_id) const {
    if (node_id >= 0 && node_id < static_cast<int>(int_to_string.size())) {
      return int_to_string[node_id];
    }
    return "";  // 无效ID
  }

  // 根据string获取int (如果不存在返回-1)
  int getNodeId(const std::string_view &node_name) const {
    auto it = string_to_int.find(node_name);
    return (it != string_to_int.end()) ? it->second : -1;
  }

  // 构建图的邻接表
  void buildGraph(const std::vector<std::shared_ptr<Arc>> &edges) {
    for (const auto &edge : edges) {
      int from_id = getOrCreateNodeId(edge->from_pin);
      int to_id = getOrCreateNodeId(edge->to_pin);

      adj_list[from_id].emplace_back(to_id, edge->delay[0]);
      rev_adj_list[to_id].emplace_back(from_id, edge->delay[0]);
      all_nodes.insert(from_id);
      all_nodes.insert(to_id);
    }
  }

  // 计算连通分量（用于快速判断两点是否连通）
  void computeComponents() {
    if (components_computed != 0) return;

    std::unordered_set<int> visited;
    int comp_id = 0;

    for (int node_id : all_nodes) {
      if (visited.find(node_id) == visited.end()) {
        // BFS遍历当前连通分量
        graph_components.resize(comp_id + 1);
        std::queue<int> q;
        q.push(node_id);
        visited.insert(node_id);
        component_id[node_id] = comp_id;
        graph_components[comp_id].push_back(node_id);

        while (!q.empty()) {
          int curr = q.front();
          q.pop();

          // 遍历所有邻居（正向和反向）
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

          // 检查反向边
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
    distance_matrixs.resize(components_computed);
    predecessor_matrixs.resize(components_computed);
  }

  void topologicalSort(int comp_id) {
    std::unordered_map<int, int> in_degree;
    std::queue<int> q;
    std::vector<int> result;

    const auto &nodes = graph_components[comp_id];

    // 初始化入度
    for (const auto &node : nodes) {
      // if (adj_list.find(node) == adj_list.end()) continue;
      in_degree[node] = 0;
    }
    for (const auto &node : adj_list) {
      for (const auto &neighbor : node.second) {
        in_degree[neighbor.first]++;
      }
    }

    // 将入度为0的节点加入队列
    for (const auto &node : in_degree) {
      if (node.second == 0) {
        q.push(node.first);
      }
    }

    // 执行拓扑排序
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      result.push_back(u);
      for (const auto &neighbor : adj_list[u]) {
        in_degree[neighbor.first]--;
        if (in_degree[neighbor.first] == 0) {
          q.push(neighbor.first);
        }
      }
    }
    graph_components[comp_id] = result;
    topo_sorted = true;
  }

  void precomputeAllPairsEfficient(int comp_id) {
    auto &distance_matrix = distance_matrixs[comp_id];
    auto &predecessor_matrix = predecessor_matrixs[comp_id];
    const auto &topological_order = graph_components[comp_id];

    // 对每个节点作为源点进行单源最短路径计算
    for (int source : topological_order) {
      std::unordered_map<int, double> &dist =
          distance_matrix[source];  // dist[target] = distance
      std::unordered_map<int, int> &prev =
          predecessor_matrix[source];  // prev[target] = predecessor

      // 按拓扑顺序处理（从源点开始）
      bool found_source = false;
      // 初始化距离
      for (int node : topological_order) {
        if (node == source) {
          found_source = true;
        }
        if (!found_source) continue;  // 跳过源点之前的节点
        dist[node] = std::numeric_limits<double>::infinity();
        prev[node] = -1;
      }
      dist[source] = 0.0;

      found_source = false;
      for (int u : topological_order) {
        if (u == source) {
          found_source = true;
        }
        if (!found_source) continue;  // 跳过源点之前的节点

        if (dist[u] == std::numeric_limits<double>::infinity()) {
          continue;
        }

        for (const auto &neighbor : adj_list[u]) {
          int v = neighbor.first;
          double weight = neighbor.second;

          if (dist[v] > dist[u] + weight) {
            dist[v] = dist[u] + weight;
            prev[v] = u;
          }
        }
      }

      // // 存储结果
      // for (int target : topological_order) {
      //   distance_matrix[source][target] = dist[target];
      //   predecessor_matrix[source][target] = prev[target];
      // }
    }
  }

  void precomputePairsEfficient(int source) {
    int comp_id = component_id[source];
    auto &distance_matrix = distance_matrixs[comp_id];
    auto &predecessor_matrix = predecessor_matrixs[comp_id];
    const auto &topological_order = graph_components[comp_id];

    std::unordered_map<int, double> &dist =
        distance_matrix[source];  // dist[target] = distance
    std::unordered_map<int, int> &prev =
        predecessor_matrix[source];  // prev[target] = predecessor

    // 按拓扑顺序处理（从源点开始）
    bool found_source = false;
    // 初始化距离
    for (int node : topological_order) {
      if (node == source) {
        found_source = true;
      }
      if (!found_source) continue;  // 跳过源点之前的节点
      dist[node] = std::numeric_limits<double>::infinity();
      prev[node] = -1;
    }
    dist[source] = 0.0;

    found_source = false;
    for (int u : topological_order) {
      if (u == source) {
        found_source = true;
      }
      if (!found_source) continue;  // 跳过源点之前的节点

      if (dist[u] == std::numeric_limits<double>::infinity()) {
        continue;
      }

      for (const auto &neighbor : adj_list[u]) {
        int v = neighbor.first;
        double weight = neighbor.second;

        if (dist[v] > dist[u] + weight) {
          dist[v] = dist[u] + weight;
          prev[v] = u;
        }
      }
    }

    // // 存储结果
    // for (int target : topological_order) {
    //   distance_matrix[source][target] = dist[target];
    //   predecessor_matrix[source][target] = prev[target];
    // }
  }

  std::unordered_map<int, CacheResult> DAGFromSource(int source_id) {
    std::unordered_map<int, CacheResult> distances;
    int comp_id = component_id[source_id];
    const std::unordered_map<int, int> &previous =
        predecessor_matrixs[comp_id][source_id];  // 记录前驱节点
    const auto &distance_matrix = distance_matrixs[comp_id];

    distances[source_id] = CacheResult(0, {});
    // fmt::print("DAG from source {}\n", source_id);

    // 重构所有路径 (转换为string路径)
    {
      ScopedTimer timer(timing_stats, "reconstruct_paths");

      for (const auto &[sink_id, distance] : distance_matrix.at(source_id)) {
        if (distance == std::numeric_limits<double>::infinity()) {
          continue;
        }
        // fmt::print("To sink_id {} with distance {}\n", sink_id, distance);
        distances[sink_id] = CacheResult(distance, {});
        auto &path = distances[sink_id].path;
        int current = sink_id;
        while (current != source_id && current != -1 &&
               previous.find(current) != previous.end()) {
          path.push_back(getNodeName(current));
          // fmt::print("Tracing to {} with id {}\n", getNodeName(current),
          //            current);
          current = previous.at(current);
        }

        if (current == source_id) {
          path.push_back(getNodeName(source_id));
          std::reverse(path.begin(), path.end());  // 反转得到正确顺序
        }
      }
    }

    return distances;
  }

  // Dijkstra算法计算从source到所有可达节点的最短距离和路径
  std::unordered_map<int, CacheResult> dijkstraFromSource(int source_id) {
    std::unordered_map<int, CacheResult> distances;
    std::unordered_map<int, int> previous;  // 记录前驱节点
    std::priority_queue<std::pair<double, int>,
                        std::vector<std::pair<double, int>>,
                        std::greater<std::pair<double, int>>>
        pq;  // {dist, node_id}

    distances[source_id] = CacheResult(0, {});
    pq.push({0, source_id});

    {
      ScopedTimer timer(timing_stats, "dijkstra_main_loop");
      while (!pq.empty()) {
        auto [curr_dist, curr_node_id] = pq.top();
        pq.pop();

        if (curr_dist > distances[curr_node_id].distance) continue;

        if (adj_list.find(curr_node_id) != adj_list.end()) {
          for (auto &[neighbor_id, edge_dist] : adj_list[curr_node_id]) {
            double new_dist = curr_dist + edge_dist;
            if (distances.find(neighbor_id) == distances.end() ||
                new_dist < distances[neighbor_id].distance) {
              distances[neighbor_id] = {new_dist, {}};
              previous[neighbor_id] = curr_node_id;  // 记录前驱节点
              pq.push({new_dist, neighbor_id});
            }
          }
        }
      }
    }

    // 重构所有路径 (转换为string路径)
    {
      ScopedTimer timer(timing_stats, "reconstruct_paths");
      for (auto &[node_id, cache_result] : distances) {
        if (node_id == source_id) {
          cache_result.path = {getNodeName(source_id)};  // 源节点到自己的路径
          continue;
        }

        std::vector<std::string_view> path;
        int current = node_id;

        // 从目标节点回溯到源节点
        while (current != source_id &&
               previous.find(current) != previous.end()) {
          path.push_back(getNodeName(current));
          current = previous[current];
        }

        if (current == source_id) {
          path.push_back(getNodeName(source_id));
          std::reverse(path.begin(), path.end());  // 反转得到正确顺序
          cache_result.path = path;
        }
      }
    }

    return distances;
  }

  // 查询两点间最短距离 (string接口)
  CacheResult queryShortestDistance(const std::string_view &from,
                                    const std::string_view &to) {
    int from_id = getNodeId(from);
    int to_id = getNodeId(to);

    // 检查节点是否存在
    if (from_id == -1 || to_id == -1) {
      return {-1, {}};  // 节点不存在
    }

    return queryShortestDistanceById(from_id, to_id);
  }

  // 查询两点间最短距离 (int接口)
  CacheResult queryShortestDistanceById(int from_id, int to_id) {
    CacheResult result;
    {
      ScopedTimer timer(timing_stats, "check_node_existence");
      if (all_nodes.find(from_id) == all_nodes.end() ||
          all_nodes.find(to_id) == all_nodes.end()) {
        return {-1, {}};
      }
    }

    {
      ScopedTimer timer(timing_stats, "check_self_loop");
      if (from_id == to_id) {
        return {0, {getNodeName(from_id)}};
      }
    }

    {
      ScopedTimer timer(timing_stats, "check_cache");
      if (distance_cache.find(from_id) != distance_cache.end() &&
          distance_cache[from_id].find(to_id) !=
              distance_cache[from_id].end()) {
        return distance_cache[from_id][to_id];
      }
    }

    {
      ScopedTimer timer(timing_stats, "compute_components_check");
      if (components_computed == 0) {
        computeComponents();
      }
    }

    {
      ScopedTimer timer(timing_stats, "check_connectivity");
      if (component_id[from_id] != component_id[to_id]) {
        distance_cache[from_id][to_id] = {-1, {}};
        return {-1, {}};
      }
    }

    if (0) {
      {
        ScopedTimer timer(timing_stats, "dijkstra_and_cache");
        if (distance_cache.find(from_id) == distance_cache.end()) {
          auto distances = dijkstraFromSource(from_id);
          distance_cache[from_id] = distances;
        }
      }
    } else {
      {
        ScopedTimer timer(timing_stats, "topo_sort");
        if (!topo_sorted) {
          for (int i = 0; i < components_computed; i++) {
            topologicalSort(i);
          }
        }
      }
      {
        ScopedTimer timer(timing_stats, "dag_init");
        // for (int i = 0; i < components_computed; i++) {
        //   precomputeAllPairsEfficient(i);
        // }
        if (distance_cache.find(from_id) == distance_cache.end()) {
          precomputePairsEfficient(from_id);
        }
      }
      {
        ScopedTimer timer(timing_stats, "dag_and_cache");
        if (distance_cache.find(from_id) == distance_cache.end()) {
          auto distances = DAGFromSource(from_id);
          distance_cache[from_id] = distances;
        }
      }
    }

    {
      ScopedTimer timer(timing_stats, "final_lookup");
      auto &from_cache = distance_cache[from_id];
      if (from_cache.find(to_id) != from_cache.end()) {
        result = from_cache[to_id];
      } else {
        result = {-1, {}};
      }
      distance_cache[from_id][to_id] = result;
    }
    return result;
  }

  // 清空缓存（节省内存）
  void clearCache() {
    distance_cache.clear();
    components_computed = false;
    component_id.clear();
  }

  // 获取图的统计信息
  void printStats() const {
    std::cout << "Number of nodes: " << all_nodes.size() << std::endl;
    std::cout << "Number of edges: ";
    int edge_count = 0;
    for (const auto &[node_id, neighbors] : adj_list) {
      edge_count += neighbors.size();
    }
    std::cout << edge_count << std::endl;
    std::cout << "Number of cached sources: " << distance_cache.size()
              << std::endl;
    std::cout << "String to int mappings: " << string_to_int.size()
              << std::endl;
  }

  // 获取所有节点名称
  std::vector<std::string_view> getAllNodeNames() const {
    std::vector<std::string_view> names;
    for (int node_id : all_nodes) {
      names.push_back(getNodeName(node_id));
    }
    return names;
  }

  // 获取所有节点ID
  std::vector<int> getAllNodeIds() const {
    std::vector<int> ids(all_nodes.begin(), all_nodes.end());
    return ids;
  }
};

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

  absl::flat_hash_map<std::string, std::shared_ptr<SparseGraphShortestPath>>
      _sparse_graph_ptrs;
};
