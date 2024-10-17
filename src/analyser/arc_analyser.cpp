#include "arc_analyser.h"

#include <fmt/color.h>
#include <fmt/ranges.h>

#include <ranges>

#include "utils/design_cons.h"
#include "utils/double_filter/double_filter.h"
#include "utils/double_filter/filter_machine.h"
#include "utils/utils.h"
#include "yaml-cpp/yaml.h"

bool arc_analyser::parse_configs() {
  bool valid = analyser::parse_configs();
  std::string delay_filter;
  collect_from_node("delay_filter", delay_filter);
  compile_double_filter(delay_filter, _delay_filter_op_code);
  std::string fanout_filter;
  collect_from_node("fanout_filter", fanout_filter);
  compile_double_filter(fanout_filter, _fanout_filter_op_code);
  return valid;
}

absl::flat_hash_set<std::string> arc_analyser::check_valid(YAML::Node &rpts) {
  absl::flat_hash_set<std::string> exist_rpts = analyser::check_valid(rpts);
  absl::flat_hash_set<std::string> valid_rpts;
  design_cons &cons = design_cons::get_instance();
  for (const auto &rpt_pair : _configs["analyse_tuples"]) {
    auto rpt_vec = rpt_pair.as<std::vector<std::string>>();
    if (!check_tuple_valid(rpt_vec, rpts)) {
      continue;
    }
    std::string rpt_0 = rpts[rpt_vec[0]]["path"].as<std::string>();
    std::string rpt_1 = rpts[rpt_vec[1]]["path"].as<std::string>();
    if (cons.get_name(rpt_0) != cons.get_name(rpt_1)) {
      fmt::print(fmt::fg(fmt::rgb(255, 0, 0)),
                 "Design names are not the same: {} {}\n", rpt_0, rpt_1);
      continue;
    }
    if (!exist_rpts.contains(rpt_vec[0]) || !exist_rpts.contains(rpt_vec[1])) {
      continue;
    }
    std::ranges::for_each(
        rpt_vec, [&](const std::string &rpt) { valid_rpts.insert(rpt); });
    _analyse_tuples.push_back(rpt_vec);
  }
  return valid_rpts;
}

void arc_analyser::open_writers() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    _arcs_writers[cmp_name] = std::make_shared<writer>(writer(cmp_name));
    _arcs_writers[cmp_name]->set_output_dir(_output_dir);
    _arcs_writers[cmp_name]->open();
  }
}

void arc_analyser::analyse() {
  open_writers();
  gen_value_map();
}

void arc_analyser::gen_value_map() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    absl::flat_hash_map<std::string_view, std::shared_ptr<Path>> pin_map;
    gen_pin2path_map(_dbs.at(rpt_pair[1]), pin_map);
    match(cmp_name, pin_map, {_dbs.at(rpt_pair[0]), _dbs.at(rpt_pair[1])});
  }
}

void arc_analyser::gen_pin2path_map(
    const std::shared_ptr<basedb> &db,
    absl::flat_hash_map<std::string_view, std::shared_ptr<Path>>
        &pin2path_map) {
  for (const auto &path : db->paths) {
    for (const auto &pin : path->path) {
      // TODO: maybe set needed
      if (!pin2path_map.contains(pin->name)) {
        pin2path_map[pin->name] = path;
      }
    }
  }
}

void arc_analyser::match(
    const std::string &cmp_name,
    const absl::flat_hash_map<std::string_view, std::shared_ptr<Path>> &pin_map,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  auto fanout_filter =
      [&](const std::tuple<std::shared_ptr<Pin>, std::shared_ptr<Pin>>
              pin_ptr_tuple) {
        if (_fanout_filter_op_code.empty()) {
          return true;
        }
        const auto &[pin_ptr, _] = pin_ptr_tuple;
        return !pin_ptr->is_input &&
               double_filter(_fanout_filter_op_code, pin_ptr->net->fanout);
      };
  auto delay_filter =
      [&](const std::tuple<std::shared_ptr<Pin>, std::shared_ptr<Pin>>
              pin_ptr_tuple) {
        if (_delay_filter_op_code.empty()) {
          return true;
        }
        const auto &[pin_ptr, _] = pin_ptr_tuple;
        return double_filter(_delay_filter_op_code, pin_ptr->incr_delay);
      };
  std::vector<YAML::Node> nodes;
  for (const auto &path : dbs[0]->paths) {
    for (const auto &pin_tuple : path->path | std::views::adjacent<2> |
                                     std::views::filter(delay_filter) |
                                     std::views::filter(fanout_filter)) {
      const auto &[pin_from, pin_to] = pin_tuple;
      if (pin_map.contains(pin_from->name) && pin_map.contains(pin_to->name)) {
        if (pin_map.at(pin_from->name) == pin_map.at(pin_to->name) &&
            !_arcs_buffer.contains({pin_from->name, pin_to->name})) {
          std::vector<std::pair<float, float>> key_locs = {pin_from->location,
                                                           pin_to->location};
          std::vector<std::pair<float, float>> value_locs;
          YAML::Node node;
          node["type"] = pin_from->is_input ? "cell arc" : "net arc";
          node["from"] = pin_from->name;
          node["to"] = pin_to->name;
          node["key"] = YAML::Node();
          node["key"]["pins"].push_back(pin_from->to_yaml());
          node["key"]["pins"].push_back(pin_to->to_yaml());
          node["value"] = YAML::Node();
          auto &value_path = pin_map.at(pin_from->name);
          bool match = true;
          double key_delay = pin_to->incr_delay;
          double value_delay = 0;

          for (const auto &value_pin :
               value_path->path |
                   std::views::drop_while(
                       [&](const std::shared_ptr<Pin> from_pin) {
                         return from_pin->name != pin_from->name;
                       }) |
                   std::views::take_while(
                       [&](const std::shared_ptr<Pin> to_pin) {
                         if (to_pin->name == pin_to->name) {
                           match = false;
                           return true;
                         }
                         return match || to_pin->name == pin_to->name;
                       })) {
            node["value"]["pins"].push_back(value_pin->to_yaml());
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
          const auto &key_endpoint = path->endpoint;
          const auto &value_endpoint = value_path->endpoint;
          if (key_endpoint == value_endpoint) {
            node["key"]["endpoint"] = key_endpoint;
            node["key"]["slack"] = path->slack;
            node["value"]["endpoint"] = value_endpoint;
            node["value"]["slack"] = value_path->slack;
            node["delta_slack"] = path->slack - value_path->slack;
          }
          _arcs_delta[std::make_pair(pin_from->name, pin_to->name)] =
              delta_delay;
          _arcs_buffer[std::make_pair(pin_from->name, pin_to->name)] = node;
        }
      }
    }
  }
  std::vector<std::pair<std::pair<std::string, std::string>, double>>
      sorted_arcs(_arcs_delta.begin(), _arcs_delta.end());
  std::sort(
      sorted_arcs.begin(), sorted_arcs.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.second > rhs.second; });
  YAML::Node arc_node;
  for (const auto &[arc, _] : sorted_arcs) {
    arc_node.push_back(_arcs_buffer[arc]);
  }
  YAML::Emitter out;
  out << arc_node;
  fmt::print(_arcs_writers[cmp_name]->out_file, "{}", out.c_str());
}
