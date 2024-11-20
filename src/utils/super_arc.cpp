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
}  // namespace super_arc
