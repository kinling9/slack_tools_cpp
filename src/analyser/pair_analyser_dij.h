#pragma once
#include <climits>
#include <functional>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pair_analyser_csv.h"

class CacheResult {
 public:
  double distance;
  std::vector<std::string> path;
  CacheResult(double dist, const std::vector<std::string> &p)
      : distance(dist), path(p) {}
  CacheResult() : distance(-1), path({}) {}
};

class SparseGraphShortestPath {
 private:
  // 邻接表存储图结构 (使用int作为节点ID)
  std::unordered_map<int, std::vector<std::pair<int, double>>>
      adj_list;  // {node_id: [(neighbor_id, dist)]}

  // 缓存已计算的最短距离，使用两级哈希表
  std::unordered_map<int, std::unordered_map<int, CacheResult>> distance_cache;

  // 存储所有节点ID
  std::unordered_set<int> all_nodes;

  // 连通分量信息
  std::unordered_map<int, int> component_id;  // node_id -> component_id
  bool components_computed = false;

  // String和int的双向映射
  std::unordered_map<std::string, int> string_to_int;  // string -> int
  std::vector<std::string> int_to_string;              // int -> string
  int next_node_id = 0;

 public:
  // 构造函数，输入边的向量
  SparseGraphShortestPath(const std::vector<std::shared_ptr<Arc>> &edges) {
    buildGraph(edges);
  }

  // 获取或创建节点的int映射
  int getOrCreateNodeId(const std::string &node_name) {
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
  std::string getNodeName(int node_id) const {
    if (node_id >= 0 && node_id < int_to_string.size()) {
      return int_to_string[node_id];
    }
    return "";  // 无效ID
  }

  // 根据string获取int (如果不存在返回-1)
  int getNodeId(const std::string &node_name) const {
    auto it = string_to_int.find(node_name);
    return (it != string_to_int.end()) ? it->second : -1;
  }

  // 构建图的邻接表
  void buildGraph(const std::vector<std::shared_ptr<Arc>> &edges) {
    for (const auto &edge : edges) {
      int from_id = getOrCreateNodeId(edge->from_pin);
      int to_id = getOrCreateNodeId(edge->to_pin);

      adj_list[from_id].emplace_back(to_id, edge->delay[0]);
      all_nodes.insert(from_id);
      all_nodes.insert(to_id);
    }
  }

  // 计算连通分量（用于快速判断两点是否连通）
  void computeComponents() {
    if (components_computed) return;

    std::unordered_set<int> visited;
    int comp_id = 0;

    for (int node_id : all_nodes) {
      if (visited.find(node_id) == visited.end()) {
        // BFS遍历当前连通分量
        std::queue<int> q;
        q.push(node_id);
        visited.insert(node_id);
        component_id[node_id] = comp_id;

        while (!q.empty()) {
          int curr = q.front();
          q.pop();

          // 遍历所有邻居（正向和反向）
          if (adj_list.find(curr) != adj_list.end()) {
            for (auto &[neighbor_id, dist] : adj_list[curr]) {
              if (visited.find(neighbor_id) == visited.end()) {
                visited.insert(neighbor_id);
                component_id[neighbor_id] = comp_id;
                q.push(neighbor_id);
              }
            }
          }

          // 检查反向边
          for (auto &[node_id, neighbors] : adj_list) {
            for (auto &[neighbor_id, dist] : neighbors) {
              if (neighbor_id == curr &&
                  visited.find(node_id) == visited.end()) {
                visited.insert(node_id);
                component_id[node_id] = comp_id;
                q.push(node_id);
              }
            }
          }
        }
        comp_id++;
      }
    }
    components_computed = true;
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

    // 重构所有路径 (转换为string路径)
    for (auto &[node_id, cache_result] : distances) {
      if (node_id == source_id) {
        cache_result.path = {getNodeName(source_id)};  // 源节点到自己的路径
        continue;
      }

      std::vector<std::string> path;
      int current = node_id;

      // 从目标节点回溯到源节点
      while (current != source_id && previous.find(current) != previous.end()) {
        path.push_back(getNodeName(current));
        current = previous[current];
      }

      if (current == source_id) {
        path.push_back(getNodeName(source_id));
        std::reverse(path.begin(), path.end());  // 反转得到正确顺序
        cache_result.path = path;
      }
    }

    return distances;
  }

  // 查询两点间最短距离 (string接口)
  CacheResult queryShortestDistance(const std::string &from,
                                    const std::string &to) {
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
    // 检查节点是否存在
    if (all_nodes.find(from_id) == all_nodes.end() ||
        all_nodes.find(to_id) == all_nodes.end()) {
      return {-1, {}};  // 节点不存在
    }

    // 如果from == to，距离为0
    if (from_id == to_id) {
      return {0, {getNodeName(from_id)}};
    }

    // 检查缓存
    if (distance_cache.find(from_id) != distance_cache.end() &&
        distance_cache[from_id].find(to_id) != distance_cache[from_id].end()) {
      return distance_cache[from_id][to_id];
    }

    // 检查连通性（可选优化）
    if (!components_computed) {
      computeComponents();
    }

    if (component_id[from_id] != component_id[to_id]) {
      distance_cache[from_id][to_id] = {-1, {}};
      return {-1, {}};  // 不连通
    }

    // 检查是否已经计算过从from出发的最短路径
    if (distance_cache.find(from_id) == distance_cache.end()) {
      // 运行Dijkstra算法
      auto distances = dijkstraFromSource(from_id);
      // 缓存结果
      distance_cache[from_id] = distances;
    }

    // 返回结果
    auto &from_cache = distance_cache[from_id];
    if (from_cache.find(to_id) != from_cache.end()) {
      return from_cache[to_id];
    } else {
      distance_cache[from_id][to_id] = {-1, {}};
      return {-1, {}};  // 不可达
    }
  }

  // 批量预计算（string接口）
  void precomputeFromNodes(const std::vector<std::string> &source_nodes) {
    for (const std::string &source : source_nodes) {
      int source_id = getNodeId(source);
      if (source_id != -1 &&
          distance_cache.find(source_id) == distance_cache.end()) {
        distance_cache[source_id] = dijkstraFromSource(source_id);
      }
    }
  }

  // 批量预计算（int接口）
  void precomputeFromNodeIds(const std::vector<int> &source_node_ids) {
    for (int source_id : source_node_ids) {
      if (all_nodes.find(source_id) != all_nodes.end() &&
          distance_cache.find(source_id) == distance_cache.end()) {
        distance_cache[source_id] = dijkstraFromSource(source_id);
      }
    }
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
  std::vector<std::string> getAllNodeNames() const {
    std::vector<std::string> names;
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
