#include "arc_analyser.h"

#include <fmt/color.h>
#include <fmt/ranges.h>

#include <nlohmann/json.hpp>
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
  collect_from_node("enable_super_arc", _enable_super_arc);
  compile_double_filter(fanout_filter, _fanout_filter_op_code);
  return valid;
}

void arc_analyser::open_writers() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    _arcs_writers[cmp_name] =
        std::make_shared<writer>(writer(fmt::format("{}.json", cmp_name)));
    _arcs_writers[cmp_name]->set_output_dir(_output_dir);
    _arcs_writers[cmp_name]->open();
  }
}

void arc_analyser::analyse() {
  if (_enable_super_arc) {
    fmt::print("Load ignore pattern\n");
    _super_arc.load_pattern("yml/super_arc_pattern.yml");
  }
  open_writers();
  gen_value_map();
}

void arc_analyser::gen_value_map() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    absl::flat_hash_map<std::pair<std::string_view, bool>,
                        std::shared_ptr<Path>>
        pin_map;
    gen_pin2path_map(_dbs.at(rpt_pair[1]), pin_map);
    match(cmp_name, pin_map, {_dbs.at(rpt_pair[0]), _dbs.at(rpt_pair[1])});
  }
}

void arc_analyser::gen_pin2path_map(
    const std::shared_ptr<basedb> &db,
    absl::flat_hash_map<std::pair<std::string_view, bool>,
                        std::shared_ptr<Path>> &pin2path_map) {
  for (const auto &path : db->paths) {
    for (const auto &pin : path->path) {
      // TODO: maybe set needed
      if (!pin2path_map.contains({pin->name, pin->rise_fall})) {
        pin2path_map[{pin->name, pin->rise_fall}] = path;
      }
    }
  }
}

void arc_analyser::match(const std::string &cmp_name,
                         absl::flat_hash_map<std::pair<std::string_view, bool>,
                                             std::shared_ptr<Path>> &pin_map,
                         const std::vector<std::shared_ptr<basedb>> &dbs) {
  _arcs_buffer.clear();
  _arcs_delta.clear();
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
  for (const auto &key_path : dbs[0]->paths) {
    for (const auto &pin_tuple :
         key_path->path | std::views::filter([&](const auto &pin) {
           return !_super_arc.check_super_arc(pin->type, pin->name);
         }) | std::views::adjacent<2> |
             std::views::filter(delay_filter) |
             std::views::filter(fanout_filter)) {
      const auto &[pin_from, pin_to] = pin_tuple;
      if (pin_map.contains({pin_from->name, pin_from->rise_fall}) &&
          pin_map.contains({pin_to->name, pin_to->rise_fall})) {
        if (pin_map.at({pin_from->name, pin_from->rise_fall}) ==
                pin_map.at({pin_to->name, pin_to->rise_fall}) &&
            !_arcs_buffer.contains({pin_from->name, pin_to->name})) {
          auto from = std::make_pair(pin_from->name, pin_from->rise_fall);
          auto to = std::make_pair(pin_to->name, pin_to->rise_fall);
          auto &value_path = pin_map.at(from);
          nlohmann::json node = {
              {"type", pin_from->is_input ? "cell arc" : "net arc"},
              {"from", from},
              {"to", to},
          };
          if (!pin_from->is_input) {
            node["net"] = pin_from->net->name;
            node["fanout"] = pin_from->net->fanout;
          }
          node["key"] = super_arc::to_json(key_path, {from.first, to.first});
          node["value"] =
              super_arc::to_json(value_path, {from.first, to.first});

          double delta_delay = node["key"]["delay"].get<double>() -
                               node["value"]["delay"].get<double>();
          node["delta_delay"] = delta_delay;
          node["delta_length"] = node["key"]["length"].get<double>() -
                                 node["value"]["length"].get<double>();
          if (node["key"]["endpoint"].get<std::string>() ==
              node["value"]["endpoint"].get<std::string>()) {
            node["delta_slack"] = key_path->slack - value_path->slack;
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
  nlohmann::json arc_node;
  for (const auto &[arc, _] : sorted_arcs) {
    arc_node.push_back(_arcs_buffer[arc]);
  }
  fmt::print(_arcs_writers[cmp_name]->out_file, "{}", arc_node.dump(2));
}
