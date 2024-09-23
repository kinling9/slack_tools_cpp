#include "hf_analyser.h"

#include <algorithm>
#include <ranges>

void hf_analyser::analyse() {
  gen_value_map();
  _writer.write();
}

void hf_analyser::gen_value_map() {
  for (const auto &[design, dbs] : _dbs) {
    absl::flat_hash_map<std::shared_ptr<Pin>, std::shared_ptr<Path>> pin_map;
    gen_pin2path_map(dbs[1], pin_map);
    match(design, pin_map, dbs);
  }
}

void hf_analyser::gen_pin2path_map(
    const std::shared_ptr<basedb> &db,
    absl::flat_hash_map<std::shared_ptr<Pin>, std::shared_ptr<Path>>
        &pin2path_map) {
  for (const auto &path : db->paths) {
    for (const auto &pin : path->path) {
      // TODO: maybe set needed
      if (!pin2path_map.contains(pin)) {
        pin2path_map[pin] = path;
      }
    }
  }
}

void hf_analyser::match(
    const std::string &design,
    const absl::flat_hash_map<std::shared_ptr<Pin>, std::shared_ptr<Path>>
        &pin_map,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  for (const auto &path : dbs[0]->paths) {
    for (const auto &pin :
         path->path | std::views::stride(2) |
             std::views::filter([](const std::shared_ptr<Pin> pin_ptr) {
               return pin_ptr->net->fanout > 100;
             })) {
      auto net = pin->net;
      if (pin_map.contains(net->pins.first) &&
          pin_map.contains(net->pins.second)) {
        if (pin_map.at(net->pins.first) == pin_map.at(net->pins.second)) {
          double total_delay = 0;
          const auto &pin_from = net->pins.first;
          const auto &pin_to = net->pins.second;
          fmt::print(_hfs_writer.out_file,
                     "detect high-fanout net from {} to {}", pin_from->name,
                     pin_to->name);
          fmt::print(_hfs_writer.out_file, "In key file:\n");
          fmt::print(_hfs_writer.out_file,
                     "pin_name: {}, incr_delay: {}, path_delay: {}\n",
                     pin_from->name, pin_from->incr_delay,
                     pin_from->path_delay);
          fmt::print(_hfs_writer.out_file,
                     "pin_name: {}, incr_delay: {}, path_delay: {}\n",
                     pin_to->name, pin_to->incr_delay, pin_to->path_delay);
          fmt::print(_hfs_writer.out_file, "In value file:\n");
          auto &value_path = pin_map.at(net->pins.first);
          for (const auto &value_pin :
               value_path->path |
                   std::views::drop_while(
                       [&](const std::shared_ptr<Pin> from_pin) {
                         return from_pin == net->pins.first;
                       }) |
                   std::views::take_while(
                       [&](const std::shared_ptr<Pin> to_pin) {
                         return to_pin == net->pins.second;
                       })) {
            fmt::print(_hfs_writer.out_file,
                       "pin_name: {}, incr_delay: {}, path_delay: {}\n",
                       value_pin->name, value_pin->incr_delay,
                       value_pin->path_delay);
            total_delay += value_pin->incr_delay;
          }
          fmt::print(_hfs_writer.out_file, "total_delay: {}\n", total_delay);
        }
      }
    }
  }
}
