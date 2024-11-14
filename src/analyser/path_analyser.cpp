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
  _csv_writer = std::make_unique<csv_writer>("path_summary.csv");
  _csv_writer->set_output_dir(_output_dir);
  return valid;
}

void path_analyser::open_writers() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    for (const auto &writer_name : {"path", "arc", "cmp"}) {
      _writers[writer_name][cmp_name] = std::make_shared<writer>(
          writer(fmt::format("{}_{}.json", cmp_name, writer_name)));
      _writers[writer_name][cmp_name]->set_output_dir(_output_dir);
      _writers[writer_name][cmp_name]->open();
    }
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
  gen_headers();
  for (const auto &rpt_pair : _analyse_tuples) {
    std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        path_maps;
    std::ranges::transform(
        rpt_pair, std::back_inserter(path_maps), [&](const auto &rpt) {
          absl::flat_hash_map<std::string, std::shared_ptr<Path>> path_map;
          gen_endpoints_map(_dbs[rpt]->type, _dbs[rpt]->paths, path_map);
          return path_map;
        });
    std::vector<std::shared_ptr<basedb>> dbs;
    std::ranges::transform(rpt_pair, std::back_inserter(dbs),
                           [&](const auto &rpt) { return _dbs[rpt]; });
    match(fmt::format("{}", fmt::join(rpt_pair, "-")), path_maps, dbs);
  }
  _csv_writer->write();
}

void path_analyser::match(
    const std::string &cmp_name,
    const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        &path_maps,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  absl::flat_hash_set<std::shared_ptr<Path>> path_set;
  _cmps_delay.clear();
  _cmps_buffer.clear();
  _paths_buffer.clear();
  _arcs_buffer.clear();
  // analyse and sort path
  for (const auto &[key, path] : path_maps[0]) {
    if (path_maps[1].contains(key) && !path_set.contains(path)) {
      path_set.emplace(path);
      _cmps_buffer[key] = path_analyse({path, path_maps[1].at(key)});
      _cmps_delay[key] = path->slack - path_maps[1].at(key)->slack;
    }
  }
  std::vector<std::pair<std::string, nlohmann::json>> sorted_cmps(
      _cmps_buffer.begin(), _cmps_buffer.end());
  std::sort(sorted_cmps.begin(), sorted_cmps.end(),
            [&](const auto &lhs, const auto &rhs) {
              return _cmps_delay[lhs.first] > _cmps_delay[rhs.first];
            });

  nlohmann::json cmp_node;
  std::unordered_map<std::string, std::size_t> filter_occur;
  std::unordered_map<std::string, std::size_t> filter_domin;
  std::ranges::for_each(_filters, [&](const auto &filter) {
    filter_occur[filter->_name] = 0;
    filter_domin[filter->_name] = 0;
  });

  // append cmp node and fill occur and dominate
  std::size_t no_issue = _cmps_buffer.size();
  for (const auto &[cmp, _] :
       sorted_cmps | std::views::filter([&](const auto &cmp) {
         return !_cmps_buffer[cmp.first].empty();
       })) {
    cmp_node.push_back(_cmps_buffer[cmp]);
    if (!_cmps_buffer[cmp].empty()) {
      filter_domin[_cmps_buffer[cmp]["domin"].get<std::string>()] += 1;
      for (const auto &filter : _cmps_buffer[cmp]["filters"]) {
        filter_occur[filter["name"]] += 1;
      }
      no_issue -= 1;
    }
  }
  fmt::print(_writers["cmp"][cmp_name]->out_file, "{}", cmp_node.dump(2));

  // write all arcs
  nlohmann::json arc_node;
  for (const auto &[arc, _] : _arcs_buffer) {
    arc_node[fmt::format("{}-{}", arc.first, arc.second)] = _arcs_buffer[arc];
  }
  fmt::print(_writers["arc"][cmp_name]->out_file, "{}", arc_node.dump(2));

  // write all paths
  nlohmann::json path_node = _paths_buffer;
  fmt::print(_writers["path"][cmp_name]->out_file, "{}", path_node.dump(2));

  // write summary
  absl::flat_hash_map<std::string, std::string> row;
  row["Design"] = dbs[0]->design;
  for (int i = 0; i < 2; i++) {
    row[fmt::format("Path num {}", i)] = std::to_string(dbs[i]->paths.size());
  }
  row["Not found"] = std::to_string(dbs[0]->paths.size() - path_set.size());
  row["Cmp name"] = cmp_name;
  for (const auto &filter : _filters) {
    row[fmt::format("{}_domin", filter->_name)] =
        fmt::format("{}", filter_domin[filter->_name]);
    row[fmt::format("{}_occur", filter->_name)] =
        fmt::format("{}", filter_occur[filter->_name]);
  }
  row["No issue"] = fmt::format("{}", no_issue);
  _csv_writer->add_row(row);
}

nlohmann::json path_analyser::path_analyse(
    const std::vector<std::shared_ptr<Path>> &paths) {
  auto key_path = paths[0];
  auto value_path = paths[1];

  // analyse path
  std::unordered_set<std::string> filter_path_set;
  std::vector<std::unordered_map<std::string, double>> attributes;
  for (auto &path : paths) {
    std::unordered_map<std::string, double> path_attributes = {
        {"length", path->get_length()},
        {"detour", path->get_detour()},
        {"cell_delay_pct", path->get_cell_delay_pct()},
        {"net_delay_pct", path->get_net_delay_pct()}};
    // add latency attributs for analyse
    for (const auto &[key, value] : path->path_params) {
      if (dm::path_param_is_data.contains(key)) {
        path_attributes[key] = dm::path_param_is_data[key] ? value : -value;
      }
    }
    attributes.push_back(path_attributes);
  }
  for (const auto &filter : _filters) {
    if (filter->_target == "path") {
      if (filter->check(attributes)) {
        filter_path_set.emplace(filter->_name);
      }
    }
  }
  attributes[0]["slack"] = key_path->slack;
  attributes[1]["slack"] = value_path->slack;
  std::unordered_map<std::string, std::vector<double>> path_attributes;
  for (const auto &[key, _] : attributes[0]) {
    path_attributes[key] = {attributes[0][key], attributes[1][key]};
  }
  _paths_buffer[key_path->endpoint] = path_attributes;

  // analyse arc
  double delta_slack = key_path->slack - value_path->slack;
  std::unordered_map<std::string,
                     std::vector<std::tuple<std::string, std::string, double>>>
      filter_arcs_map;  // [from, to, delta_delay]
  std::unordered_set<std::string> pin_set;
  std::ranges::transform(
      value_path->path, std::inserter(pin_set, pin_set.end()),
      [](const std::shared_ptr<Pin> &pin) { return pin->name; });
  for (const auto &pin_tuple :
       key_path->path | std::views::adjacent<2> |
           std::views::filter([&](const auto &pin_tuple) {
             const auto &[pin_from, pin_to] = pin_tuple;
             return pin_set.contains(pin_from->name) &&
                    pin_set.contains(pin_to->name);
           })) {
    {
      const auto &[pin_from, pin_to] = pin_tuple;
      if (!_arcs_buffer.contains({pin_from->name, pin_to->name})) {
        // general attributes
        nlohmann::json node = {
            {"type", pin_from->is_input ? "cell arc" : "net arc"},
            {"from", pin_from->name},
            {"to", pin_to->name},
            {"count", 1},
        };
        if (!pin_from->is_input) {
          node["net"] = pin_from->net->name;
          node["fanout"] = pin_from->net->fanout;
        }

        // key attributes
        std::vector<std::pair<float, float>> key_locs = {pin_from->location,
                                                         pin_to->location};
        node["key"] = nlohmann::json::object();
        node["key"]["pins"] = nlohmann::json::array();
        node["key"]["pins"].push_back(pin_from->to_json());
        node["key"]["pins"].push_back(pin_to->to_json());
        double key_delay = pin_to->incr_delay;
        node["key"]["delay"] = key_delay;
        float key_len = manhattan_distance(key_locs);
        node["key"]["length"] = key_len;

        // value attributes
        node["value"] = nlohmann::json::object();
        std::vector<std::pair<float, float>> value_locs;
        double value_delay = 0;

        // using bool to take while + 1
        bool match = true;
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
          if (value_pin->name != pin_from->name) {
            value_delay += value_pin->incr_delay;
          }
        }
        node["value"]["delay"] = value_delay;
        float value_len = manhattan_distance(value_locs);
        node["value"]["length"] = value_len;

        // delta attributes
        double delta_delay = key_delay - value_delay;
        node["delta_delay"] = delta_delay;
        node["delta_length"] = key_len - value_len;
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
              // NOTE: each arc corresponds to only one filter
              // TODO: maybe multi filter for one arc
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
  if (filter_arcs_map.empty() && filter_path_set.empty()) {
    return nlohmann::json();
  }
  nlohmann::json result;
  result["endpoint"] = key_path->endpoint;
  result["key_slack"] = key_path->slack;
  result["value_slack"] = value_path->slack;
  result["delta_slack"] = delta_slack;
  result["filters"] = nlohmann::json::array();
  result["domin"] = "";
  double max_pct = 0.;
  for (const auto &filter_name : filter_path_set) {
    result["filters"].push_back({{"name", filter_name}});
  }
  for (const auto &[filter_name, filter_arcs] : filter_arcs_map) {
    nlohmann::json filter;
    filter["name"] = filter_name;
    filter["arcs"] = filter_arcs;
    double total_delta = 0;
    for (const auto &[from, to, delta_delay] : filter_arcs) {
      total_delta += delta_delay;
    }
    double delta_pct = total_delta / delta_slack * 100;
    filter["total_delta"] = total_delta;
    filter["delta_percent"] = fmt::format("{:.2f}%", delta_pct);

    // NOTE: domin is valid only when delta_pct larger than 0
    if (delta_pct >= max_pct) {
      max_pct = delta_pct;
      result["domin"] = filter_name;
    }
    result["filters"].push_back(filter);
  }
  return result;
}

void path_analyser::gen_headers() {
  // output headers
  std::vector<std::string> headers = {"Design", "Cmp name", "Path num 0",
                                      "Path num 1", "Not found"};
  for (const auto &filter : _filters) {
    headers.push_back(fmt::format("{}_domin", filter->_name));
    headers.push_back(fmt::format("{}_occur", filter->_name));
  }
  headers.push_back("No issue");
  _csv_writer->set_headers(headers);
}
