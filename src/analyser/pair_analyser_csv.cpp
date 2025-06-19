#include "pair_analyser_csv.h"

#include <absl/container/flat_hash_set.h>
#include <fmt/ranges.h>

#include <thread>

#include "dm/dm.h"

void pair_analyser_csv::csv_match(
    const std::string &cmp_name,
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        &arcs,
    absl::flat_hash_map<std::pair<std::string_view, bool>,
                        std::shared_ptr<Path>> &pin_map,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  absl::flat_hash_map<std::tuple<std::string, bool, std::string, bool>,
                      nlohmann::json>
      arcs_buffer;
  for (const auto &[arc_cell, arc_net] : arcs) {
    auto pin_from = arc_cell->from_pin;
    auto pin_inter = arc_cell->to_pin;
    auto pin_to = arc_net->to_pin;
    auto from = std::make_pair(pin_from, false);
    auto to = std::make_pair(pin_to, false);
    auto arc_tuple = std::make_tuple(pin_from, false, pin_to, false);
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
        node["key"] = {
            {"pins", nlohmann::json::array()},
            {"delay", arc_cell->delay[0] + arc_net->delay[0]},
        };
        node["key"]["pins"].push_back({"name", pin_from});
        node["key"]["pins"].push_back({"name", pin_inter});
        node["key"]["pins"].push_back({"name", pin_to});

        node["value"] =
            super_arc::to_json(value_path, arc_tuple, _enable_rise_fall);

        double delta_delay = node["key"]["delay"].get<double>() -
                             node["value"]["delay"].get<double>();
        node["delta_delay"] = delta_delay;
        // node["delta_length"] = node["key"]["length"].get<double>() -
        //                        node["value"]["length"].get<double>();
        // if (node["key"]["endpoint"].get<std::string>() ==
        //     node["value"]["endpoint"].get<std::string>()) {
        // }
        arcs_buffer[arc_tuple] = node;
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

void pair_analyser_csv::gen_arc_tuples(
    const std::shared_ptr<basedb> &db,
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        &arcs) {
  // fmt::print("cell_arcs size {}, net_arcs size {}\n",
  //            db->get_cell_arcs().size(), db->get_net_arcs().size());
  // for (const auto &[net_from, net_arcs] : db->get_net_arcs()) {
  //   for (const auto &[net_to, net_arc] : net_arcs) {
  //     fmt::print("Net arc: {} - {}\n", net_from, net_to);
  //   }
  // }
  for (const auto &[cell_to, cell_arcs] : db->get_cell_arcs()) {
    if (db->get_net_arcs().contains(cell_to)) {
      auto net_arc_map = db->get_net_arcs().at(cell_to);
      if (net_arc_map.empty()) {
        fmt::print("No net arcs for cell {}\n", cell_to);
        continue;  // Skip if there are no net arcs for this cell
      }
      for (const auto &[cell_from, cell_arc] : cell_arcs) {
        for (const auto &[net_from, net_arc] : net_arc_map) {
          std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>> arc_tuple(
              cell_arc, net_arc);
          if (arcs.contains(arc_tuple)) {
            fmt::print("Skip arc tuple: {} - {}\n", cell_arc->from_pin,
                       net_arc->to_pin);
            continue;  // Skip if the arc tuple already exists
          } else {
            arcs.insert(arc_tuple);
            // fmt::print("Add arc tuple: {} - {}\n", cell_arc->from_pin,
            //            net_arc->to_pin);
          }
        }
      }
    }
  }
}

void pair_analyser_csv::analyse() {
  if (_enable_rise_fall) {
    fmt::print("Enable rise fall check\n");
    _rf_checker.set_enable_rise_fall(true);
  }
  open_writers();
  std::vector<std::thread> threads;
  fmt::print("Analyse tuples: {}\n", fmt::join(_analyse_tuples, ", "));
  for (const auto &rpt_pair : _analyse_tuples) {
    threads.emplace_back([this, rpt_pair]() {
      std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
      absl::flat_hash_set<
          std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
          arcs;
      absl::flat_hash_map<std::pair<std::string_view, bool>,
                          std::shared_ptr<Path>>
          pin_map;
      gen_arc_tuples(_dbs.at(rpt_pair[0]), arcs);
      gen_pin2path_map(_dbs.at(rpt_pair[1]), pin_map);
      csv_match(cmp_name, arcs, pin_map,
                {_dbs.at(rpt_pair[0]), _dbs.at(rpt_pair[1])});
    });
  }
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}
