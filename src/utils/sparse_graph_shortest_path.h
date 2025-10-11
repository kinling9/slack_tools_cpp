#pragma once

#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dm/dm.h"
#include "utils/cache_result.h"

class sparse_graph_shortest_path {
 private:
  // 邻接表存储图结构 (使用int作为节点ID)
  std::unordered_map<int, std::vector<std::pair<int, double>>>
      adj_list;  // {node_id: [(neighbor_id, dist)]}
  std::unordered_map<int, std::vector<std::pair<int, double>>>
      rev_adj_list;  // {node_id: [(neighbor_id, dist)]}

  // 存储所有节点ID
  std::unordered_set<int> all_nodes;

  // 连通分量信息
  std::vector<int> component_id;  // node_id -> component_id
  int components_computed = 0;

  // String和int的双向映射
  std::unordered_map<std::string_view, int> string_to_int;  // string -> int
  std::vector<std::string_view> int_to_string;              // int -> string
  int next_node_id = 0;

 public:
  std::unordered_map<std::string, long long> timing_stats;
  std::vector<std::vector<int>> graph_components;
  std::vector<std::unordered_map<int, int>> topological_orders;
  std::vector<int> graph_sizes;

  std::mutex graph_mutex;  // Protects adj_list, rev_adj_list

  // Node set and component info
  std::mutex component_mutex;  // Protects component-related state

 public:
  // 构造函数，输入边的向量
  sparse_graph_shortest_path(const std::vector<std::shared_ptr<Arc>> &edges);

  // 获取或创建节点的int映射
  int getOrCreateNodeId(const std::string_view &node_name);

  // 根据int获取string
  std::string_view getNodeName(int node_id) const;

  // 根据string获取int (如果不存在返回-1)
  int getNodeId(const std::string_view &node_name) const;

  // 构建图的邻接表
  void buildGraph(const std::vector<std::shared_ptr<Arc>> &edges);

  // 计算连通分量（用于快速判断两点是否连通）
  void computeComponents();

  void topologicalSort(int comp_id);

  // 查询两点间最短距离 (string接口)
  cache_result queryShortestDistance(const std::string_view &from,
                                     const std::string_view &to);

  // 查询两点间最短距离 (int接口)
  cache_result queryShortestDistanceById(int from_id, int to_id);
  cache_result dijkstra_topo(int from_id, int to_id, int comp_id) const;
  cache_result reconstruct_path(int from_id, int to_id,
                                const std::unordered_map<int, int> &parent,
                                double distance) const;

  // 获取图的统计信息
  void printStats() const;
};
