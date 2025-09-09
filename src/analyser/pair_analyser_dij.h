#pragma once
#include <absl/strings/match.h>

#include <chrono>
#include <climits>
#include <functional>
#include <iostream>
#include <mutex>
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
    std::lock_guard<std::mutex> lock(get_timer_mutex());
    m_accum[m_name] += duration;
  }

  std::mutex &get_timer_mutex() {
    static std::mutex timer_mutex;
    return timer_mutex;
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

  std::mutex graph_mutex;  // Protects adj_list, rev_adj_list

  // Cache for shortest paths - heavily read, written during Dijkstra
  std::mutex cache_mutex;  // Support multiple concurrent readers

  // Node set and component info
  std::mutex component_mutex;  // Protects component-related state

  std::mutex precomp_mutex;  // Protects precomputation results

 public:
  // 构造函数，输入边的向量
  SparseGraphShortestPath(const std::vector<std::shared_ptr<Arc>> &edges);

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

  void precomputePairsEfficient(int source);

  void DAGFromSource(int source_id);

  // Dijkstra算法计算从source到所有可达节点的最短距离和路径
  std::unordered_map<int, CacheResult> dijkstraFromSource(int source_id);

  // 查询两点间最短距离 (string接口)
  CacheResult queryShortestDistance(const std::string_view &from,
                                    const std::string_view &to);

  // 查询两点间最短距离 (int接口)
  CacheResult queryShortestDistanceById(int from_id, int to_id);

  // 清空缓存（节省内存）
  void clearCache();

  // 获取图的统计信息
  void printStats() const;

  // 获取所有节点名称
  std::vector<std::string_view> getAllNodeNames() const;

  // 获取所有节点ID
  std::vector<int> getAllNodeIds() const;
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

  nlohmann::json create_pin_node(
      const std::string &name, bool is_input, double incr_delay,
      const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db);
  void process_arc_segment(
      int t, size_t begin_idx, size_t end_idx,
      const absl::flat_hash_set<
          std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>> &arcs,
      const std::vector<std::string> &rpt_pair,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_key,
      const std::unordered_map<std::string, std::shared_ptr<Pin>>
          &csv_pin_db_value,
      std::vector<std::map<std::tuple<std::string, bool, std::string, bool>,
                           nlohmann::json>>
          thread_buffers);
  absl::flat_hash_map<std::string, std::shared_ptr<SparseGraphShortestPath>>
      _sparse_graph_ptrs;
};
