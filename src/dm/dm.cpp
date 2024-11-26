#include "dm/dm.h"

#include "utils/utils.h"

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
  node["is_input"] = is_input;
  node["trans"] = trans;
  node["cell"] = cell;
  return node;
}

nlohmann::json Net::to_json() {
  nlohmann::json node = {
      {"name", name},
      {"fanout", fanout},
      {"cap", cap},
  };
  return node;
}

double Path::get_length() {
  if (length.has_value()) {
    return length.value();
  }
  std::vector<std::pair<float, float>> locs;
  for (const auto &pin : path | std::views::filter([](const auto &pin) {
                           return pin->location != std::make_pair(0., 0.);
                         })) {
    locs.push_back(pin->location);
  }
  double len = manhattan_distance(locs);
  length = len;
  return len;
}

double Path::get_detour() {
  if (detour.has_value()) {
    return detour.value();
  }
  std::vector<std::pair<float, float>> all_locs;
  for (const auto &pin : path | std::views::filter([](const auto &pin) {
                           return pin->location != std::make_pair(0., 0.);
                         })) {
    all_locs.push_back(pin->location);
  }
  std::vector<std::pair<float, float>> locs = {all_locs.front(),
                                               all_locs.back()};
  double len = manhattan_distance(locs);
  double det = get_length() / len;
  detour = det;
  return det;
}

double Path::get_delay() {
  if (total_delay.has_value()) {
    return total_delay.value();
  }
  double delay = 0;
  for (const auto &pin : path) {
    delay += pin->incr_delay;
  }
  total_delay = delay;
  return delay;
}

double Path::get_cell_delay_pct() {
  if (cell_delay_pct.has_value()) {
    return cell_delay_pct.value();
  }
  double cell_delay = 0;
  for (const auto &pin : path | std::views::filter([](const auto &pin) {
                           return !pin->is_input;
                         })) {
    cell_delay += pin->incr_delay;
  }
  double total_delay = get_delay();
  double pct = cell_delay / total_delay;
  cell_delay_pct = pct;
  return pct;
}

double Path::get_net_delay_pct() {
  if (net_delay_pct.has_value()) {
    return net_delay_pct.value();
  }
  double net_delay = 0;
  for (const auto &pin : path | std::views::filter([](const auto &pin) {
                           return pin->is_input;
                         })) {
    net_delay += pin->incr_delay;
  }
  double total_delay = get_delay();
  double pct = net_delay / total_delay;
  net_delay_pct = pct;
  return pct;
}

void basedb::update_loc_from_map(
    const absl::flat_hash_map<std::string, std::pair<double, double>>
        &loc_map) {
  for (const auto &path : paths) {
    for (const auto &pin : path->path) {
      std::size_t pos = pin->name.find_last_of('/');
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
