#include "arc_analyser.h"

#include <ranges>

#include "utils/double_filter/filter_machine.h"

arc_analyser::arc_analyser(
    const configs &configs,
    const absl::flat_hash_map<std::string, std::vector<std::shared_ptr<basedb>>>
        &dbs)
    : analyser(configs), _dbs(dbs) {
  for (const auto &[design, _] : _dbs) {
    _arcs_writers[design] = std::make_shared<writer>(writer(design + ".arcs"));
    _arcs_writers[design]->set_output_dir(_configs.output_dir);
    _arcs_writers[design]->open();
  }
}

void arc_analyser::analyse() { gen_value_map(); }

void arc_analyser::gen_value_map() {
  for (const auto &[design, dbs] : _dbs) {
    absl::flat_hash_map<std::string_view, std::shared_ptr<Path>> pin_map;
    gen_pin2path_map(dbs[1], pin_map);
    match(design, pin_map, dbs);
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
    const std::string &design,
    const absl::flat_hash_map<std::string_view, std::shared_ptr<Path>> &pin_map,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  auto fanout_filter = [&](const std::shared_ptr<Pin> pin_ptr) {
    if (_configs.slack_filter_op_code.empty()) {
      return true;
    }
    return pin_ptr->is_input &&
           double_filter(_configs.slack_filter_op_code, pin_ptr->net->fanout);
  };
  auto delay_filter = [&](const std::shared_ptr<Pin> pin_ptr) {
    if (_configs.delay_filter_op_code.empty()) {
      return true;
    }
    return double_filter(_configs.delay_filter_op_code, pin_ptr->incr_delay);
  };
  for (const auto &path : dbs[0]->paths) {
    for (const auto &pin : path->path | std::views::filter(delay_filter) |
                               std::views::filter(fanout_filter)) {
      auto net = pin->net;
      if (pin_map.contains(net->pins.first->name) &&
          pin_map.contains(net->pins.second->name)) {
        if (pin_map.at(net->pins.first->name) ==
            pin_map.at(net->pins.second->name)) {
          const auto &pin_from = net->pins.first;
          const auto &pin_to = net->pins.second;
          fmt::print(_arcs_writers[design]->out_file,
                     "\ndetect high-fanout net from {} to {}\n", pin_from->name,
                     pin_to->name);
          fmt::print(_arcs_writers[design]->out_file, "In key file:\n");
          fmt::print(_arcs_writers[design]->out_file,
                     "pin_name: {}, incr_delay: {}, path_delay: {}\n",
                     pin_from->name, pin_from->incr_delay,
                     pin_from->path_delay);
          fmt::print(_arcs_writers[design]->out_file,
                     "pin_name: {}, incr_delay: {}, path_delay: {}\n",
                     pin_to->name, pin_to->incr_delay, pin_to->path_delay);
          fmt::print(_arcs_writers[design]->out_file, "In value file:\n");
          auto &value_path = pin_map.at(net->pins.first->name);
          bool match = true;
          double key_delay = pin_to->incr_delay;
          double value_delay = 0;
          for (const auto &value_pin :
               value_path->path |
                   std::views::drop_while(
                       [&](const std::shared_ptr<Pin> from_pin) {
                         return from_pin->name != net->pins.first->name;
                       }) |
                   std::views::take_while(
                       [&](const std::shared_ptr<Pin> to_pin) {
                         if (to_pin->name == net->pins.second->name) {
                           match = false;
                           return true;
                         }
                         return match || to_pin->name == net->pins.second->name;
                       })) {
            fmt::print(_arcs_writers[design]->out_file,
                       "pin_name: {}, incr_delay: {}, path_delay: {}\n",
                       value_pin->name, value_pin->incr_delay,
                       value_pin->path_delay);
            if (value_pin->name == net->pins.first->name) {
              continue;
            }
            value_delay += value_pin->incr_delay;
          }
          double delta_delay = key_delay - value_delay;
          fmt::print(_arcs_writers[design]->out_file,
                     "key_delay: {}, value_delay: {}, delta_delay: {}\n",
                     key_delay, value_delay, delta_delay);
        }
      }
    }
  }
}
