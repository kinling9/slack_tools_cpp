#include "pair_analyser_csv.h"

#include <absl/container/flat_hash_set.h>
#include <fmt/ranges.h>

#include <thread>

#include "dm/dm.h"
#include "utils/utils.h"

void pair_analyser_csv::csv_match(
    const std::string &cmp_name,
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        &arcs,
    absl::flat_hash_map<std::pair<std::string_view, bool>,
                        std::shared_ptr<Path>> &pin_map,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db) {
  absl::flat_hash_map<std::tuple<std::string, bool, std::string, bool>,
                      nlohmann::json>
      arcs_buffer;
  auto createPinNode = [&csv_pin_db](const std::string &name, bool is_input,
                                     double incr_delay) {
    auto node = nlohmann::json{{"name", name},
                               {"is_input", is_input},
                               {"incr_delay", incr_delay},
                               {"rf", false}};
    if (!csv_pin_db.empty()) {
      auto pin_it = csv_pin_db.find(name);
      if (pin_it != csv_pin_db.end()) {
        auto pin = pin_it->second;
        node["path_delay"] = pin->path_delay;
        node["location"] =
            nlohmann::json::array({pin->location.first, pin->location.second});
        node["trans"] = pin->trans;
        node["cap"] = pin->cap.value_or(0.);
      }
    }
    return node;
  };
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
            {"key",
             {
                 {"pins", nlohmann::json::array()},
                 {"delay", arc_cell->delay[0] + arc_net->delay[0]},
             }},
        };

        node["key"]["pins"].push_back(createPinNode(pin_from, true, 0));
        node["key"]["pins"].push_back(
            createPinNode(pin_inter, false, arc_cell->delay[0]));
        node["key"]["pins"].push_back(
            createPinNode(pin_to, false, arc_net->delay[0]));
        node["value"] =
            super_arc::to_json(value_path, arc_tuple, _enable_rise_fall);

        if (csv_pin_db.contains(pin_inter)) {
          node["key"]["slack"] =
              csv_pin_db.at(pin_inter)->path_slack.value_or(0.0);
          node["delta_slack"] = node["key"]["slack"].get<double>() -
                                node["value"]["slack"].get<double>();
        }
        if (!csv_pin_db.empty()) {
          std::vector<std::pair<float, float>> locs;
          bool valid_location = true;
          for (const auto &pin : node["key"]["pins"]) {
            if (!pin.contains("location")) {
              valid_location = false;
              break;
            }
            locs.push_back({pin["location"][0].get<double>(),
                            pin["location"][1].get<double>()});
          }
          if (!valid_location) {
            node["key"]["length"] = manhattan_distance(locs);
            node["delta_length"] = node["key"]["length"].get<double>() -
                                   node["value"]["length"].get<double>();
          }
        }
        double delta_delay = node["key"]["delay"].get<double>() -
                             node["value"]["delay"].get<double>();
        node["delta_delay"] = delta_delay;
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
  for (const auto &[cell_to, cell_arcs] : db->cell_arcs) {
    if (db->net_arcs.contains(cell_to)) {
      auto &net_arc_map = db->net_arcs.at(cell_to);
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
      csv_match(cmp_name, arcs, pin_map, _dbs.at(rpt_pair[0])->pins);
    });
  }
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}
