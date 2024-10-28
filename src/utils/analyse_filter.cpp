#include "utils/analyse_filter.h"

#include <fmt/ranges.h>

#include <iostream>

#include "utils/double_filter/double_filter.h"
#include "utils/double_filter/filter_machine.h"
#include "utils/utils.h"

static std::function<bool(const std::vector<double> &)> generate(
    const std::vector<double> &op_code, std::unordered_set<std::string> type) {
  return [op_code, type](const std::vector<double> &attributes) -> bool {
    if (attributes.size() != 2) {
      return false;
    }
    double num = attributes[0];
    if (type.contains("delta")) {
      num = attributes[1] - attributes[0];
    }
    if (type.contains("abs")) {
      num = std::abs(num);
    }
    if (type.contains("percent")) {
      num = num / attributes[1];
    }
    return double_filter(op_code, num);
  };
}

analyse_filter::analyse_filter(const YAML::Node &filter_node, std::string &name,
                               std::string &target)
    : _name(name), _target(target) {
  parse_filter(filter_node);
}

void analyse_filter::parse_filter(const YAML::Node &filter_node) {
  for (const auto &filter : filter_node) {
    filter_group fg;
    collect_from_node(filter, "attribute", fg.name);
    std::string filter_str;
    collect_from_node(filter, "filter", filter_str);
    compile_double_filter(filter_str, fg.op_code);
    std::vector<std::string> filter_types;
    collect_from_node(filter, "type", filter_types);
    fg.filter = generate(fg.op_code, std::unordered_set(filter_types.begin(),
                                                        filter_types.end()));
    _filters.push_back(fg);
  }
}

bool analyse_filter::check(
    const std::vector<std::unordered_map<std::string, double>> &attributes)
    const {
  for (const auto &filter : _filters) {
    if (!filter.filter(
            {attributes[0].at(filter.name), attributes[1].at(filter.name)})) {
      return false;
    }
  }
  return true;
}
