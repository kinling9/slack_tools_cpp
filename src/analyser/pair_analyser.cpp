#include "pair_analyser.h"

#include "utils/double_filter/double_filter.h"
#include "utils/double_filter/filter_machine.h"

void pair_analyser::match(
    const std::string &cmp_name,
    const absl::flat_hash_map<std::pair<std::string_view, bool>,
                              std::shared_ptr<Path>> &pin_map,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  absl::flat_hash_map<std::tuple<std::string, bool, std::string, bool>,
                      nlohmann::json>
      arcs_buffer;
  auto fanout_filter =
      [&](const std::tuple<std::shared_ptr<Pin>, std::shared_ptr<Pin>,
                           std::shared_ptr<Pin>>
              pin_ptr_tuple) {
        if (_fanout_filter_op_code.empty()) {
          return true;
        }
        const auto &[_0, pin_ptr, _1] = pin_ptr_tuple;
        return !pin_ptr->is_input &&
               double_filter(_fanout_filter_op_code, pin_ptr->net->fanout);
      };
  auto delay_filter =
      [&](const std::tuple<std::shared_ptr<Pin>, std::shared_ptr<Pin>,
                           std::shared_ptr<Pin>>
              pin_ptr_tuple) {
        if (_delay_filter_op_code.empty()) {
          return true;
        }
        const auto &[pin_ptr0, pin_ptr1, _] = pin_ptr_tuple;
        return double_filter(_delay_filter_op_code,
                             pin_ptr0->incr_delay + pin_ptr1->incr_delay);
      };
  auto drop_filter =
      [&](const std::tuple<std::shared_ptr<Pin>, std::shared_ptr<Pin>,
                           std::shared_ptr<Pin>>
              pin_ptr_tuple) -> bool {
    const auto &[pin0, pin1, pin2] = pin_ptr_tuple;
    if (pin0->is_input) {
      return true;
    } else {
      return false;
    }
  };
  for (const auto &key_path : dbs[0]->paths) {
    for (const auto &pin_tuple : key_path->path | std::views::adjacent<3> |
                                     std::views::filter(drop_filter) |
                                     std::views::filter(delay_filter) |
                                     std::views::filter(fanout_filter)) {
      const auto &[pin_from, pin_inter, pin_to] = pin_tuple;
      auto arc_tuple = std::make_tuple(
          pin_from->name, _rf_checker.check(pin_from->rise_fall), pin_to->name,
          _rf_checker.check(pin_to->rise_fall));
      auto from = std::make_pair(pin_from->name,
                                 _rf_checker.check(pin_from->rise_fall));
      auto to =
          std::make_pair(pin_to->name, _rf_checker.check(pin_to->rise_fall));
      if (pin_map.contains(from) && pin_map.contains(to)) {
        if (pin_map.at(from) == pin_map.at(to) &&
            !arcs_buffer.contains(arc_tuple)) {
          auto &value_path = pin_map.at(from);
          nlohmann::json node = {
              {"type", "pair arc"},
              {"from", fmt::format("{} {}", from.first,
                                   from.second ? "(rise)" : "(fall)")},
              {"to",
               fmt::format("{} {}", to.first, to.second ? "(rise)" : "(fall)")},
          };
          node["key"] =
              super_arc::to_json(key_path, arc_tuple, _enable_rise_fall);
          node["key"]["net"] = pin_inter->net->name;
          node["key"]["fanout"] = pin_inter->net->fanout;
          node["value"] =
              super_arc::to_json(value_path, arc_tuple, _enable_rise_fall);

          double delta_delay = node["key"]["delay"].get<double>() -
                               node["value"]["delay"].get<double>();
          node["delta_delay"] = delta_delay;
          node["delta_length"] = node["key"]["length"].get<double>() -
                                 node["value"]["length"].get<double>();
          if (node["key"]["endpoint"].get<std::string>() ==
              node["value"]["endpoint"].get<std::string>()) {
            node["delta_slack"] = key_path->slack - value_path->slack;
          }
          arcs_buffer[arc_tuple] = node;
        }
      }
    }
  }
  nlohmann::json arc_node;
  for (const auto &[arc, _] : arcs_buffer) {
    arc_node[fmt::format(
        "{} {}-{} {}", std::get<0>(arc), std::get<1>(arc) ? "(rise)" : "(fall)",
        std::get<2>(arc), std::get<3>(arc) ? "(rise)" : "(fall)")] =
        arcs_buffer[arc];
  }
  fmt::print(_arcs_writers[cmp_name]->out_file, "{}", arc_node.dump(2));
}
