#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "yaml-cpp/yaml.h"

struct filter_group {
  std::string name;
  std::function<bool(const std::vector<double> &)> filter;
  std::vector<double> op_code;
};

class analyse_filter {
 public:
  analyse_filter(const YAML::Node &filter_node, std::string &name,
                 std::string &target);
  ~analyse_filter() = default;

  void parse_filter(const YAML::Node &filter_node);
  bool check(const std::vector<std::unordered_map<std::string, double>>
                 &attributes) const;

 public:
  std::string _name;
  std::string _target;

 private:
  std::vector<filter_group> _filters;
};
