#include "dm/dm.h"

#include <cstdlib>
#include <fstream>

#include "utils/utils.h"
#include "yyjson.h"

YAML::Node Pin::to_yaml() {
  YAML::Node node;
  // pack for arc analyser
  node["name"] = name;
  node["incr_delay"] = incr_delay.value_or(0.);
  node["path_delay"] = path_delay.value_or(0.);
  node["location"] = YAML::Node();
  node["location"].push_back(location.first);
  node["location"].push_back(location.second);
  node.SetStyle(YAML::EmitterStyle::Flow);
  return node;
}

nlohmann::json Pin::to_json() {
  nlohmann::json node;
  node["name"] = name;
  node["incr_delay"] = incr_delay.value_or(0.);
  node["path_delay"] = path_delay.value_or(0.);
  node["location"] = nlohmann::json::array({location.first, location.second});
  node["is_input"] = is_input;
  node["trans"] = trans.value_or(0.);
  if (cell.has_value()) {
    node["cell"] = cell.value();
  }
  node["rf"] = rise_fall.value_or(false);

  if (pta_buf.has_value()) {
    node["pta_buf"] = pta_buf.value();
  }

  if (pta_net.has_value()) {
    node["pta_net"] = pta_net.value();
  }

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
  if (all_locs.size() < 2) {
    return 0;
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
    delay += pin->incr_delay.value_or(0.);
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
    cell_delay += pin->incr_delay.value_or(0.);
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
    net_delay += pin->incr_delay.value_or(0.);
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

void basedb::serialize_to_json(const std::string &output_path) const {
  nlohmann::json node;
  node["design"] = design;
  node["type"] = type;
  node["paths"] = nlohmann::json::array();
  for (const auto &path : paths) {
    nlohmann::json path_node;
    path_node["clock"] = path->clock;
    path_node["slack"] = path->slack;
    path_node["path_params"] = nlohmann::json::object();
    for (const auto &[key, value] : path->path_params) {
      path_node["path_params"][key] = value;
    }
    path_node["path_group"] = path->group;
    path_node["length"] = path->get_length();
    path_node["detour"] = path->get_detour();
    path_node["cell_delay_pct"] = path->get_cell_delay_pct();
    path_node["net_delay_pct"] = path->get_net_delay_pct();
    path_node["path"] = nlohmann::json::array();
    for (const auto &pin : path->path) {
      path_node["path"].push_back(pin->to_json());
      if (pin->is_input && pin->net.has_value()) {
        path_node["path"].back()["net"] = pin->net.value()->to_json();
      }
    }
    path_node["startpoint"] = path->startpoint;
    path_node["endpoint"] = path->endpoint;
    node["paths"].push_back(path_node);
  }
  std::ofstream ofs(output_path);
  ofs << std::setw(2) << node << std::endl;
}

namespace {
yyjson_mut_val *net_to_yyjson(yyjson_mut_doc *doc,
                              const std::shared_ptr<Net> &net) {
  yyjson_mut_val *obj = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(doc, obj, "name", net->name.c_str());
  yyjson_mut_obj_add_int(doc, obj, "fanout", net->fanout);
  yyjson_mut_obj_add_real(doc, obj, "cap", net->cap);
  return obj;
}

yyjson_mut_val *pin_to_yyjson(yyjson_mut_doc *doc,
                              const std::shared_ptr<Pin> &pin) {
  yyjson_mut_val *obj = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(doc, obj, "name", pin->name.c_str());
  yyjson_mut_obj_add_real(doc, obj, "incr_delay", pin->incr_delay.value_or(0.));
  yyjson_mut_obj_add_real(doc, obj, "path_delay", pin->path_delay.value_or(0.));

  yyjson_mut_val *loc_arr = yyjson_mut_arr(doc);
  yyjson_mut_arr_add_real(doc, loc_arr, pin->location.first);
  yyjson_mut_arr_add_real(doc, loc_arr, pin->location.second);
  yyjson_mut_obj_add_val(doc, obj, "location", loc_arr);

  yyjson_mut_obj_add_bool(doc, obj, "is_input", pin->is_input);
  yyjson_mut_obj_add_real(doc, obj, "trans", pin->trans.value_or(0.));

  if (pin->cell.has_value()) {
    yyjson_mut_obj_add_str(doc, obj, "cell", pin->cell.value().c_str());
  }

  yyjson_mut_obj_add_bool(doc, obj, "rf", pin->rise_fall.value_or(false));

  if (pin->pta_buf.has_value()) {
    yyjson_mut_obj_add_real(doc, obj, "pta_buf", pin->pta_buf.value());
  }

  if (pin->pta_net.has_value()) {
    yyjson_mut_obj_add_real(doc, obj, "pta_net", pin->pta_net.value());
  }

  return obj;
}
}  // namespace

void basedb::serialize_to_yyjson(const std::string &output_path) const {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_obj_add_str(doc, root, "design", design.c_str());
  yyjson_mut_obj_add_str(doc, root, "type", type.c_str());

  yyjson_mut_val *paths_arr = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, root, "paths", paths_arr);

  for (const auto &path : paths) {
    yyjson_mut_val *path_node = yyjson_mut_obj(doc);
    yyjson_mut_arr_append(paths_arr, path_node);

    yyjson_mut_obj_add_str(doc, path_node, "clock", path->clock.c_str());
    yyjson_mut_obj_add_real(doc, path_node, "slack", path->slack);

    yyjson_mut_val *path_params_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, path_node, "path_params", path_params_obj);
    for (const auto &[key, value] : path->path_params) {
      yyjson_mut_obj_add_real(doc, path_params_obj, key.c_str(), value);
    }

    yyjson_mut_obj_add_str(doc, path_node, "path_group", path->group.c_str());
    yyjson_mut_obj_add_real(doc, path_node, "length", path->get_length());
    yyjson_mut_obj_add_real(doc, path_node, "detour", path->get_detour());
    yyjson_mut_obj_add_real(doc, path_node, "cell_delay_pct",
                            path->get_cell_delay_pct());
    yyjson_mut_obj_add_real(doc, path_node, "net_delay_pct",
                            path->get_net_delay_pct());

    yyjson_mut_val *pin_arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, path_node, "path", pin_arr);

    for (const auto &pin : path->path) {
      yyjson_mut_val *pin_obj = pin_to_yyjson(doc, pin);
      yyjson_mut_arr_append(pin_arr, pin_obj);

      if (pin->is_input && pin->net.has_value()) {
        yyjson_mut_val *net_obj = net_to_yyjson(doc, pin->net.value());
        yyjson_mut_obj_add_val(doc, pin_obj, "net", net_obj);
      }
    }

    yyjson_mut_obj_add_str(doc, path_node, "startpoint",
                           path->startpoint.c_str());
    yyjson_mut_obj_add_str(doc, path_node, "endpoint", path->endpoint.c_str());
  }

  yyjson_write_err err;
  const char *json =
      yyjson_mut_write_opts(doc, YYJSON_WRITE_PRETTY, NULL, NULL, &err);
  if (json) {
    std::ofstream ofs(output_path);
    ofs << json;
    free((void *)json);
  }

  yyjson_mut_doc_free(doc);
}
