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
  // 邻接表存储图结构
  std::unordered_map<std::string, std::vector<std::pair<std::string, double>>>
      adj_list;  // {node: [(neighbor, dist)]}

  // 缓存已计算的最短距离，使用两级哈希表
  std::unordered_map<std::string, std::unordered_map<std::string, CacheResult>>
      distance_cache;

  // 存储所有节点
  std::unordered_set<std::string> all_nodes;

  // 连通分量信息
  std::unordered_map<std::string, int> component_id;  // node -> component_id
  bool components_computed = false;

 public:
  // 构造函数，输入边的向量
  SparseGraphShortestPath(const std::vector<std::shared_ptr<Arc>> &edges) {
    buildGraph(edges);
  }

  // 构建图的邻接表
  void buildGraph(const std::vector<std::shared_ptr<Arc>> &edges) {
    for (const auto &edge : edges) {
      adj_list[edge->from_pin].emplace_back(edge->to_pin, edge->delay[0]);
      all_nodes.insert(edge->from_pin);
      all_nodes.insert(edge->to_pin);
    }
    for (const auto &node : all_nodes) {
      fmt::print("Node: {}\n", node);
    }
  }

  // 计算连通分量（用于快速判断两点是否连通）
  void computeComponents() {
    if (components_computed) return;

    std::unordered_set<std::string> visited;
    int comp_id = 0;

    for (auto node : all_nodes) {
      if (visited.find(node) == visited.end()) {
        // BFS遍历当前连通分量
        std::queue<std::string> q;
        q.push(node);
        visited.insert(node);
        component_id[node] = comp_id;

        while (!q.empty()) {
          std::string curr = q.front();
          q.pop();

          // 遍历所有邻居（正向和反向）
          if (adj_list.find(curr) != adj_list.end()) {
            for (auto &neighbor : adj_list[curr]) {
              if (visited.find(neighbor.first) == visited.end()) {
                visited.insert(neighbor.first);
                component_id[neighbor.first] = comp_id;
                q.push(neighbor.first);
              }
            }
          }

          // 检查反向边
          for (auto &[node_id, neighbors] : adj_list) {
            for (auto &[neighbor, dist] : neighbors) {
              if (neighbor == curr && visited.find(node_id) == visited.end()) {
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

  // // Dijkstra算法计算从source到所有可达节点的最短距离
  // std::unordered_map<std::string, double> dijkstraFromSource(
  //     std::string source) {
  //   std::unordered_map<std::string, double> distances;
  //   std::priority_queue<std::pair<double, std::string>,
  //                       std::vector<std::pair<double, std::string>>,
  //                       std::greater<std::pair<double, std::string>>>
  //       pq;  // {dist, node}
  //
  //   distances[source] = 0;
  //   pq.push({0, source});
  //
  //   while (!pq.empty()) {
  //     auto [curr_dist, curr_node] = pq.top();
  //     pq.pop();
  //
  //     if (curr_dist > distances[curr_node]) continue;
  //
  //     if (adj_list.find(curr_node) != adj_list.end()) {
  //       for (auto &[neighbor, edge_dist] : adj_list[curr_node]) {
  //         double new_dist = curr_dist + edge_dist;
  //
  //         if (distances.find(neighbor) == distances.end() ||
  //             new_dist < distances[neighbor]) {
  //           distances[neighbor] = new_dist;
  //           pq.push({new_dist, neighbor});
  //         }
  //       }
  //     }
  //   }
  //   // may add length data
  //
  //   return distances;
  // }

  // Dijkstra算法计算从source到所有可达节点的最短距离和路径
  std::unordered_map<std::string, CacheResult>

  dijkstraFromSource(std::string source) {
    std::unordered_map<std::string, CacheResult> distances;
    std::unordered_map<std::string, std::string> previous;  // 记录前驱节点
    std::priority_queue<std::pair<double, std::string>,
                        std::vector<std::pair<double, std::string>>,
                        std::greater<std::pair<double, std::string>>>
        pq;  // {dist, node}

    distances[source] = CacheResult(0, {});
    pq.push({0, source});

    while (!pq.empty()) {
      auto [curr_dist, curr_node] = pq.top();
      pq.pop();

      if (curr_dist > distances[curr_node].distance) continue;

      if (adj_list.find(curr_node) != adj_list.end()) {
        for (auto &[neighbor, edge_dist] : adj_list[curr_node]) {
          double new_dist = curr_dist + edge_dist;
          if (distances.find(neighbor) == distances.end() ||
              new_dist < distances[neighbor].distance) {
            distances[neighbor] = {new_dist, {}};
            previous[neighbor] = curr_node;  // 记录前驱节点
            pq.push({new_dist, neighbor});
          }
        }
      }
    }

    // 重构所有路径
    for (const auto &[node, dist] : distances) {
      if (node == source) {
        distances[node].path = {source};  // 源节点到自己的路径
        continue;
      }

      std::vector<std::string> path;
      std::string current = node;

      // 从目标节点回溯到源节点
      while (current != source && previous.find(current) != previous.end()) {
        path.push_back(current);
        current = previous[current];
      }

      if (current == source) {
        path.push_back(source);
        std::reverse(path.begin(), path.end());  // 反转得到正确顺序
        distances[node].path = path;
      }
    }

    return distances;
  }

  // 查询两点间最短距离
  CacheResult queryShortestDistance(std::string from, std::string to) {
    // 检查节点是否存在
    if (all_nodes.find(from) == all_nodes.end() ||
        all_nodes.find(to) == all_nodes.end()) {
      return {-1, {}};  // 节点不存在
    }

    // 如果from == to，距离为0
    if (from == to) return {0, {}};

    // 检查缓存
    if (distance_cache.find(from) != distance_cache.end() &&
        distance_cache[from].find(to) != distance_cache[from].end()) {
      return distance_cache[from][to];
    }

    // 检查连通性（可选优化）
    if (!components_computed) {
      computeComponents();
    }

    if (component_id[from] != component_id[to]) {
      distance_cache[from][to] = {-1, {}};
      return {-1, {}};  // 不连通
    }

    // 检查是否已经计算过从from出发的最短路径
    if (distance_cache.find(from) == distance_cache.end()) {
      // 运行Dijkstra算法
      auto distances = dijkstraFromSource(from);

      // 缓存结果
      distance_cache[from] = distances;
    }

    // 返回结果
    auto &from_cache = distance_cache[from];
    if (from_cache.find(to) != from_cache.end()) {
      return from_cache[to];
    } else {
      distance_cache[from][to] = {-1, {}};
      return {-1, {}};  // 不可达
    }
  }

  // 批量预计算（可选）
  void precomputeFromNodes(const std::vector<std::string> &source_nodes) {
    for (const std::string &source : source_nodes) {
      if (distance_cache.find(source) == distance_cache.end()) {
        distance_cache[source] = dijkstraFromSource(source);
      }
    }
  }

  // 清空缓存（节省内存）
  void clearCache() { distance_cache.clear(); }

  // 获取图的统计信息
  void printStats() const {
    std::cout << "number of node: " << all_nodes.size() << std::endl;
    std::cout << "number of edges: ";
    int edge_count = 0;
    for (const auto &[node, neighbors] : adj_list) {
      edge_count += neighbors.size();
    }
    std::cout << edge_count << std::endl;
    std::cout << "number of cached sources: " << distance_cache.size()
              << std::endl;
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
