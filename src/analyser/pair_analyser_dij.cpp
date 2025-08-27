#include "pair_analyser_dij.h"

#include <fmt/ranges.h>

#include <thread>

#include "utils/utils.h"

void pair_analyser_dij::analyse() {
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
      gen_arc_tuples(_dbs.at(rpt_pair[0]), arcs);
      init_graph(_dbs.at(rpt_pair[1]), rpt_pair[1]);
      csv_match(rpt_pair, arcs, _dbs.at(rpt_pair[0])->pins,
                _dbs.at(rpt_pair[1])->pins);
      // for (const auto &arc : arcs) {
      //   auto from = std::get<0>(arc)->from_pin;
      //   auto to = std::get<1>(arc)->to_pin;
      //   CacheResult distance = {-1, {}};
      //   if (_sparse_graph_ptrs.contains(rpt_pair[1])) {
      //     distance = _sparse_graph_ptrs.at(rpt_pair[1])
      //                    ->queryShortestDistance(from, to);
      //     fmt::print("Distance from {} to {} is {}\n", from, to,
      //                distance.distance);
      //     fmt::print("Path: {}\n", fmt::join(distance.path, " -> "));
      //   } else {
      //     fmt::print("No graph for type {}\n", _dbs.at(rpt_pair[1])->type);
      //   }
      // }
    });
  }
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void pair_analyser_dij::init_graph(const std::shared_ptr<basedb> &db,
                                   std::string name) {
  if (db == nullptr) {
    fmt::print("DB is nullptr, skip\n");
    return;
  }
  auto graph = std::make_shared<SparseGraphShortestPath>(db->all_arcs);
  _sparse_graph_ptrs[name] = graph;
  graph->printStats();
}

void pair_analyser_dij::csv_match(
    const std::vector<std::string> &rpt_pair,
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        &arcs,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value) {
  absl::flat_hash_map<std::tuple<std::string, bool, std::string, bool>,
                      nlohmann::json>
      arcs_buffer;
  auto createPinNode =
      [](const std::string &name, bool is_input, double incr_delay,
         const std::unordered_map<std::string, std::shared_ptr<Pin>>
             &csv_pin_db) {
        auto node = nlohmann::json{{"name", name},
                                   {"is_input", is_input},
                                   {"incr_delay", incr_delay},
                                   {"rf", false}};
        if (!csv_pin_db.empty()) {
          auto pin_it = csv_pin_db.find(name);
          if (pin_it != csv_pin_db.end()) {
            auto pin = pin_it->second;
            node["path_delay"] = pin->path_delay;
            node["location"] = nlohmann::json::array(
                {pin->location.first, pin->location.second});
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
    if (!_sparse_graph_ptrs.contains(rpt_pair[1])) {
      fmt::print("No graph for type {}\n", rpt_pair[1]);
      return;
    }
    auto connect_check = _sparse_graph_ptrs[rpt_pair[1]]->queryShortestDistance(
        pin_from, pin_to);
    if (connect_check.distance < 0) {
      fmt::print("No connection from {} to {}, skip\n", pin_from, pin_to);
      continue;
    }
    nlohmann::json node = {
        {"type", "pair arc"},
        {"from",
         fmt::format("{} {}", from.first, from.second ? "(rise)" : "(fall)")},
        {"to", fmt::format("{} {}", to.first, to.second ? "(rise)" : "(fall)")},
        {"key",
         {
             {"pins", nlohmann::json::array()},
             {"delay", arc_cell->delay[0] + arc_net->delay[0]},
         }},
        {"value",
         {
             {"pins", nlohmann::json::array()},
             {"delay", connect_check.distance},
         }}};

    node["key"]["pins"].push_back(
        createPinNode(pin_from, true, 0, csv_pin_db_key));
    node["key"]["pins"].push_back(
        createPinNode(pin_inter, false, arc_cell->delay[0], csv_pin_db_key));
    node["key"]["pins"].push_back(
        createPinNode(pin_to, true, arc_net->delay[0], csv_pin_db_key));

    node["value"]["pins"].push_back(
        createPinNode(pin_from, true, 0, csv_pin_db_value));

    bool is_cell_arc = true;
    auto value_db = _dbs.at(rpt_pair[1]);
    for (const auto &pin_tuple : connect_check.path | std::views::adjacent<2>) {
      const auto &[mid_from, mid_to] = pin_tuple;
      std::shared_ptr<Arc> mid_arc = nullptr;
      fmt::print("Mid arc from {} to {}\n", mid_from, mid_to);
      mid_arc = is_cell_arc ? value_db->get_cell_arcs()[mid_from][mid_to]
                            : value_db->get_net_arcs()[mid_from][mid_to];
      is_cell_arc = !is_cell_arc;
      node["value"]["pins"].push_back(createPinNode(
          mid_to, !is_cell_arc, mid_arc->delay[0], csv_pin_db_value));
    }
    if (arc_net->fanout.has_value()) {
      node["key"]["fanout"] = arc_net->fanout.value();
    }
    bool valid_location = true;
    if (csv_pin_db_key.contains(pin_inter)) {
      node["key"]["slack"] =
          csv_pin_db_key.at(pin_inter)->path_slack.value_or(0.0);
      node["value"]["slack"] =
          csv_pin_db_value.contains(pin_inter)
              ? csv_pin_db_value.at(pin_inter)->path_slack.value_or(0.0)
              : 0.0;
      node["delta_slack"] = node["key"]["slack"].get<double>() -
                            node["value"]["slack"].get<double>();
    }
    std::vector<std::pair<float, float>> locs_key;
    std::vector<std::pair<float, float>> locs_value;
    if (!csv_pin_db_key.empty()) {
      for (const auto &pin : node["key"]["pins"]) {
        if (!pin.contains("location")) {
          valid_location = false;
          break;
        }
        locs_key.push_back({pin["location"][0].get<double>(),
                            pin["location"][1].get<double>()});
      }
    }
    if (valid_location && !csv_pin_db_value.empty()) {
      for (const auto &pin : node["value"]["pins"]) {
        if (!pin.contains("location")) {
          valid_location = false;
          break;
        }
        locs_value.push_back({pin["location"][0].get<double>(),
                              pin["location"][1].get<double>()});
      }
    }
    if (valid_location) {
      node["key"]["length"] = manhattan_distance(locs_key);
      node["value"]["length"] = manhattan_distance(locs_value);
      node["delta_length"] = node["key"]["length"].get<double>() -
                             node["value"]["length"].get<double>();
    }
    double delta_delay = node["key"]["delay"].get<double>() -
                         node["value"]["delay"].get<double>();
    node["delta_delay"] = delta_delay;
    arcs_buffer[arc_tuple] = node;
  }
  nlohmann::json arc_node;
  for (const auto &[arc, _] : arcs_buffer) {
    arc_node[fmt::format(
        "{} {}-{} {}", std::get<0>(arc), std::get<1>(arc) ? "(rise)" : "(fall)",
        std::get<2>(arc), std::get<3>(arc) ? "(rise)" : "(fall)")] =
        arcs_buffer[arc];
  }

  std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
  fmt::print(_arcs_writers[cmp_name]->out_file, "{}", arc_node.dump(2));
}
