#include "utils/super_arc.h"

#include <ranges>

#include "utils/utils.h"

namespace super_arc {

nlohmann::json to_json(const std::shared_ptr<Path> path,
                       std::pair<std::string, std::string> names) {
  nlohmann::json node;
  bool match = true;
  double delay = 0;
  std::vector<std::pair<float, float>> locs;
  node["pins"] = nlohmann::json::array();
  for (const auto &value_pin :
       path->path |
           std::views::drop_while([&](const std::shared_ptr<Pin> from_pin) {
             return from_pin->name != names.first;
           }) |
           std::views::take_while([&](const std::shared_ptr<Pin> to_pin) {
             if (to_pin->name == names.second) {
               match = false;
               return true;
             }
             return match || to_pin->name == names.second;
           })) {
    node["pins"].push_back(value_pin->to_json());
    locs.push_back(value_pin->location);
    if (value_pin->name == names.first) {
      continue;
    }
    delay += value_pin->incr_delay;
  }
  node["delay"] = delay;
  node["length"] = manhattan_distance(locs);
  node["endpoint"] = path->endpoint;
  node["slack"] = path->slack;
  return node;
}

void super_arc_pattern::load_pattern(const std::string &pattern_yml) {
  YAML::Node super_arc_pattern;
  try {
    super_arc_pattern = YAML::LoadFile(pattern_yml);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::exit(1);
  }
  for (const auto &tool_pattern : super_arc_pattern) {
    for (const auto &pattern : tool_pattern.second) {
      std::unique_ptr<RE2> pattern_split =
          std::make_unique<RE2>(pattern.as<std::string>());
      _patterns[tool_pattern.first.as<std::string>()].emplace_back(
          std::move(pattern_split));
    }
  }
  fmt::print(
      "super_arc match enabled, finsh loading super_arc pattern from {}\n",
      pattern_yml);
  _enable_super_arc = true;
}

bool super_arc_pattern::check_super_arc(const std::string &tool,
                                        const std::string &line) const {
  if (!_enable_super_arc) {
    return false;
  }
  if (!_patterns.contains(tool)) {
    return false;
  }
  for (const auto &pattern : _patterns.at(tool)) {
    if (RE2::PartialMatch(line, *pattern)) {
      return true;
    }
  }
  return false;
}

}  // namespace super_arc
