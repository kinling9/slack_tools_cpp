#include "path_analyser.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <ranges>

#include "utils/utils.h"

bool path_analyser::parse_configs() {
  bool valid = analyser::parse_configs();
  collect_from_node("enable_mbff", _enable_mbff);
  auto patterns = _configs["analyse_patterns"];
  for (const auto &pattern : patterns) {
    std::string name = pattern["name"].as<std::string>();
    std::string target = pattern["target"].as<std::string>();
    auto filters = pattern["filters"];
    _filters.push_back(std::make_unique<analyse_filter>(filters, name, target));
  }
  return valid;
}

void path_analyser::open_writers() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    _paths_writers[cmp_name] =
        std::make_shared<writer>(writer(fmt::format("{}_path.json", cmp_name)));
    _paths_writers[cmp_name]->set_output_dir(_output_dir);
    _paths_writers[cmp_name]->open();
    _arcs_writers[cmp_name] =
        std::make_shared<writer>(writer(fmt::format("{}_arc.json", cmp_name)));
    _arcs_writers[cmp_name]->set_output_dir(_output_dir);
    _arcs_writers[cmp_name]->open();
  }
}

void path_analyser::gen_endpoints_map(
    const std::string &type, std::ranges::input_range auto &&paths,
    absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map) {
  std::function<std::vector<std::string>(std::shared_ptr<Path>)> key_generator;
  key_generator =
      [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
    return _mbff.get_ff_names(type, path->endpoint);
  };
  std::ranges::for_each(paths, [&](const std::shared_ptr<Path> &path) {
    auto keys = key_generator(path);
    for (const auto &key : keys) {
      path_map[key] = path;
    }
  });
}

void path_analyser::analyse() {
  if (_enable_mbff) {
    fmt::print("Load MBFF pattern\n");
    _mbff.load_pattern("yml/mbff_pattern.yml");
  }
  open_writers();
  for (const auto &rpt_pair : _analyse_tuples) {
    std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        path_maps;
    std::ranges::transform(
        rpt_pair, std::back_inserter(path_maps), [&](const auto &rpt) {
          absl::flat_hash_map<std::string, std::shared_ptr<Path>> path_map;
          gen_endpoints_map(_dbs[rpt]->type, _dbs[rpt]->paths, path_map);
          return path_map;
        });
    match(fmt::format("{}", fmt::join(rpt_pair, "-")), path_maps);
  }
}

void path_analyser::match(
    const std::string &cmp_name,
    const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        &path_maps) {
  absl::flat_hash_set<std::shared_ptr<Path>> path_set;
  _paths_buffer.clear();
  _paths_delay.clear();
  for (const auto &[key, path] : path_maps[0]) {
    if (path_maps[1].contains(key) && !path_set.contains(path)) {
      path_set.emplace(path);
      _paths_buffer[key] = path_analyse({path, path_maps[1].at(key)});
      _paths_delay[key] = path->slack - path_maps[1].at(key)->slack;
    }
  }
  std::vector<std::pair<std::string, nlohmann::json>> sorted_paths(
      _paths_buffer.begin(), _paths_buffer.end());
  std::sort(sorted_paths.begin(), sorted_paths.end(),
            [&](const auto &lhs, const auto &rhs) {
              return _paths_delay[lhs.first] > _paths_delay[rhs.first];
            });
  nlohmann::json path_node;
  for (const auto &[path, _] : sorted_paths) {
    path_node.push_back(_paths_buffer[path]);
  }
  fmt::print(_paths_writers[cmp_name]->out_file, "{}", path_node.dump(2));
  std::vector<std::pair<std::pair<std::string, std::string>, double>>
      sorted_arcs;
  std::ranges::transform(
      _arcs_buffer, std::back_inserter(sorted_arcs),
      [](const std::pair<std::pair<std::string, std::string>, nlohmann::json>
             &arc) {
        return std::make_pair(arc.first,
                              arc.second["delta_delay"].get<double>() *
                                  arc.second["count"].get<std::size_t>());
      });
  std::sort(
      sorted_arcs.begin(), sorted_arcs.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.second > rhs.second; });
  nlohmann::json arc_node;
  for (const auto &[arc, _] : sorted_arcs) {
    arc_node.push_back(_arcs_buffer[arc]);
  }
  fmt::print(_arcs_writers[cmp_name]->out_file, "{}", arc_node.dump(2));
}

nlohmann::json path_analyser::path_analyse(
    const std::vector<std::shared_ptr<Path>> &paths) {
  std::unordered_map<std::string,
                     std::vector<std::tuple<std::string, std::string, double>>>
      filter_arcs_map;
  std::unordered_set<std::string> pin_set;
  auto key_path = paths[0];
  auto value_path = paths[1];
  double delta_slack = key_path->slack - value_path->slack;
  // analyse arc
  std::ranges::transform(
      value_path->path, std::inserter(pin_set, pin_set.end()),
      [](const std::shared_ptr<Pin> &pin) { return pin->name; });
  for (const auto &pin_tuple : key_path->path | std::views::adjacent<2>) {
    const auto &[pin_from, pin_to] = pin_tuple;
    if (pin_set.contains(pin_from->name) && pin_set.contains(pin_to->name)) {
      if (!_arcs_buffer.contains({pin_from->name, pin_to->name})) {
        std::vector<std::pair<float, float>> key_locs = {pin_from->location,
                                                         pin_to->location};
        std::vector<std::pair<float, float>> value_locs;
        nlohmann::json node = {
            {"type", pin_from->is_input ? "cell arc" : "net arc"},
            {"from", pin_from->name},
            {"to", pin_to->name},
        };
        node["key"] = nlohmann::json::object();
        node["key"]["pins"] = nlohmann::json::array();
        node["key"]["pins"].push_back(pin_from->to_json());
        node["key"]["pins"].push_back(pin_to->to_json());
        node["value"] = nlohmann::json::object();
        bool match = true;
        double key_delay = pin_to->incr_delay;
        double value_delay = 0;

        for (const auto &value_pin :
             value_path->path |
                 std::views::drop_while(
                     [&](const std::shared_ptr<Pin> from_pin) {
                       return from_pin->name != pin_from->name;
                     }) |
                 std::views::take_while([&](const std::shared_ptr<Pin> to_pin) {
                   if (to_pin->name == pin_to->name) {
                     match = false;
                     return true;
                   }
                   return match || to_pin->name == pin_to->name;
                 })) {
          node["value"]["pins"].push_back(value_pin->to_json());
          value_locs.push_back(value_pin->location);
          if (value_pin->name == pin_from->name) {
            continue;
          }
          value_delay += value_pin->incr_delay;
        }
        double delta_delay = key_delay - value_delay;
        node["delta_delay"] = delta_delay;
        node["key"]["delay"] = key_delay;
        node["value"]["delay"] = value_delay;
        float key_len = manhattan_distance(key_locs);
        float value_len = manhattan_distance(value_locs);
        node["delta_length"] = key_len - value_len;
        node["key"]["length"] = key_len;
        node["value"]["length"] = value_len;
        node["count"] = 1;
        _arcs_buffer[std::make_pair(pin_from->name, pin_to->name)] = node;

        std::vector<std::unordered_map<std::string, double>> attributes;
        for (const auto &iter : {"key", "value"}) {
          attributes.push_back({{"delay", node[iter]["delay"]},
                                {"length", node[iter]["length"]},
                                {"fanout", pin_from->net->fanout}});
        }
        for (const auto &filter : _filters) {
          if (node["type"] == filter->_target) {
            if (filter->check(attributes)) {
              filter_arcs_map[filter->_name].push_back(
                  {pin_from->name, pin_to->name, delta_delay});
              _filter_cache[std::make_pair(pin_from->name, pin_to->name)] =
                  filter->_name;
              break;
            }
          }
        }
      } else {
        auto name_pair = std::make_pair(pin_from->name, pin_to->name);
        std::size_t count = _arcs_buffer[name_pair]["count"].get<std::size_t>();
        _arcs_buffer[name_pair]["count"] = count + 1;
        if (_filter_cache.contains(name_pair)) {
          filter_arcs_map[_filter_cache[name_pair]].push_back(
              {name_pair.first, name_pair.second,
               _arcs_buffer[name_pair]["delta_delay"]});
        }
      }
    }
  }
  nlohmann::json result;
  result["endpoint"] = key_path->endpoint;
  // result["slack"] = key_path->slack;
  result["filters"] = nlohmann::json::array();
  for (const auto &[filter_name, filter_arcs] : filter_arcs_map) {
    nlohmann::json filter;
    filter["name"] = filter_name;
    filter["arcs"] = filter_arcs;
    double total_delta = 0;
    for (const auto &[from, to, delta_delay] : filter_arcs) {
      total_delta += delta_delay;
    }
    double delta_percent =
        std::abs(delta_slack) < 1e-9 ? 0 : total_delta / delta_slack * 100;
    filter["total_delta"] = total_delta;
    filter["delta_percent"] = delta_percent;
    result["filters"].push_back(filter);
  }
  return result;
}
