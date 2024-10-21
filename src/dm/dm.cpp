#include "dm/dm.h"

YAML::Node Pin::to_yaml() {
  YAML::Node node;
  // pack for arc analyser
  node["name"] = name;
  node["incr_delay"] = incr_delay;
  node["path_delay"] = path_delay;
  node["location"] = YAML::Node();
  node["location"].push_back(location.first);
  node["location"].push_back(location.second);
  node.SetStyle(YAML::EmitterStyle::Flow);
  return node;
}

nlohmann::json Pin::to_json() {
  nlohmann::json node;
  node["name"] = name;
  node["incr_delay"] = incr_delay;
  node["path_delay"] = path_delay;
  node["location"] = nlohmann::json::array({location.first, location.second});
  return node;
}

void basedb::update_loc_from_map(
    const absl::flat_hash_map<std::string, std::pair<double, double>>
        &loc_map) {
  for (const auto &path : paths) {
    for (const auto &pin : path->path) {
      size_t pos = pin->name.find_last_of('/');
      if (pos == std::string::npos) {
        continue;
      }
      std::string cell_name = pin->name.substr(0, pos);
      if (loc_map.contains(cell_name)) {
        pin->location = loc_map.at(cell_name);
      }
    }
  }
}
