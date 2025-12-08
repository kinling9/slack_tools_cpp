#include "pair_analyser_graph.h"

#include <fmt/ranges.h>

#include <algorithm>
#include <thread>

#include "utils/cache_result.h"
#include "utils/utils.h"

void pair_analyser_graph::analyse() {
  // if (_enable_rise_fall) {
  //   fmt::print("Enable rise fall check\n");
  //   _rf_checker.set_enable_rise_fall(true);
  // }
  open_writers();
  fmt::print("Analyse tuples: {}\n", fmt::join(_analyse_tuples, ", "));
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        arcs;
    gen_arc_tuples(_dbs.at(rpt_pair[0]), arcs);
    init_graph(_dbs.at(rpt_pair[1]), rpt_pair[1]);
    csv_match(rpt_pair, arcs, _dbs.at(rpt_pair[0])->pins,
              _dbs.at(rpt_pair[1])->pins);
  }
}

void pair_analyser_graph::init_graph(const std::shared_ptr<basedb> &db,
                                     std::string name) {
  if (db == nullptr) {
    fmt::print("DB is nullptr, skip\n");
    return;
  }
  auto graph = std::make_shared<sparse_graph_shortest_path>();
  graph->build_graph(db->all_arcs);
  _sparse_graph_ptrs[name] = graph;
  graph->print_stats();
}

nlohmann::json pair_analyser_graph::create_pin_node(
    const std::string &name, bool is_input, double incr_delay,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db) {
  nlohmann::json node;
  node["name"] = name;
  node["is_input"] = is_input;
  node["incr_delay"] = incr_delay;
  node["rf"] = false;
  if (!csv_pin_db.empty()) {
    if (auto pin_it = csv_pin_db.find(name); pin_it != csv_pin_db.end()) {
      const auto &pin = pin_it->second;
      node["path_delay"] = pin->path_delay;
      node["location"] = {pin->location.first, pin->location.second};
      node["trans"] = pin->trans;
      node["cap"] = pin->cap.value_or(0.);
    }
  }
  return node;
}

void pair_analyser_graph::process_arc_segment(
    int t, size_t begin_idx, size_t end_idx,
    const absl::flat_hash_set<
        std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>> &arcs,
    const std::vector<std::string> &rpt_pair,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value,
    std::vector<std::map<std::tuple<std::string, bool, std::string, bool>,
                         nlohmann::json>> &thread_buffers) {
  if (!_sparse_graph_ptrs.contains(rpt_pair[1])) {
    fmt::print("No graph for type {}\n", rpt_pair[1]);
    return;
  }

  auto it = arcs.begin();
  std::advance(it, begin_idx);
  auto end_it = arcs.begin();
  std::advance(end_it, end_idx);

  for (; it != end_it; ++it) {
    const auto &[arc_cell, arc_net] = *it;
    const auto &pin_from = arc_cell->from_pin;
    const auto &pin_inter = arc_cell->to_pin;
    const auto &pin_to = arc_net->to_pin;
    auto from = std::make_pair(pin_from, false);
    auto to = std::make_pair(pin_to, false);
    auto arc_tuple = std::make_tuple(pin_from, false, pin_to, false);

    auto connect_check =
        _sparse_graph_ptrs[rpt_pair[1]]->query_shortest_distance(pin_from,
                                                                 pin_to);
    if (connect_check.distance < 0) {
      fmt::print("No connection from {} to {}, skip\n", pin_from, pin_to);
      continue;
    }

    auto max_cell_delay = std::max(arc_cell->delay[0], arc_cell->delay[1]);
    auto max_net_delay = std::max(arc_net->delay[0], arc_net->delay[1]);

    nlohmann::json node;
    node["type"] = "pair arc";
    node["from"] =
        fmt::format("{} {}", from.first, from.second ? "(rise)" : "(fall)");
    node["to"] =
        fmt::format("{} {}", to.first, to.second ? "(rise)" : "(fall)");

    // Pre-calculate delay
    const double total_delay = max_cell_delay + max_net_delay;

    // Build key section
    nlohmann::json key_pins = nlohmann::json::array();
    key_pins.push_back(create_pin_node(pin_from, true, 0, csv_pin_db_key));
    key_pins.push_back(
        create_pin_node(pin_inter, false, max_cell_delay, csv_pin_db_key));
    key_pins.push_back(
        create_pin_node(pin_to, true, max_net_delay, csv_pin_db_key));

    node["key"] = {{"pins", std::move(key_pins)}, {"delay", total_delay}};

    node["value"] = {{"pins", nlohmann::json::array()},
                     {"delay", connect_check.distance}};

    node["value"]["pins"].push_back(
        create_pin_node(pin_from, true, 0, csv_pin_db_value));

    bool is_cell_arc = true;
    auto value_db = _dbs.at(rpt_pair[1]);
    for (const auto &pin_tuple : connect_check.path | std::views::adjacent<2>) {
      const auto &[mid_from_view, mid_to_view] = pin_tuple;
      std::string mid_from = std::string(mid_from_view);
      std::string mid_to = std::string(mid_to_view);
      std::shared_ptr<Arc> &mid_arc =
          is_cell_arc ? value_db->cell_arcs[mid_from][mid_to]
                      : value_db->net_arcs[mid_from][mid_to];
      is_cell_arc = !is_cell_arc;
      node["value"]["pins"].push_back(create_pin_node(
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
        locs_key.emplace_back(pin["location"][0].get<double>(),
                              pin["location"][1].get<double>());
      }
    }

    if (valid_location && !csv_pin_db_value.empty()) {
      for (const auto &pin : node["value"]["pins"]) {
        if (!pin.contains("location")) {
          valid_location = false;
          break;
        }
        locs_value.emplace_back(pin["location"][0].get<double>(),
                                pin["location"][1].get<double>());
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

    // Store in thread-local buffer
    thread_buffers[t][arc_tuple] = std::move(node);
  }
}

void pair_analyser_graph::csv_match(
    const std::vector<std::string> &rpt_pair,
    absl::flat_hash_set<std::tuple<std::shared_ptr<Arc>, std::shared_ptr<Arc>>>
        &arcs,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value) {
  absl::flat_hash_map<std::tuple<std::string, bool, std::string, bool>,
                      nlohmann::json>
      arcs_buffer;

  unsigned int num_threads =
      std::max(1u, std::min(2u, static_cast<unsigned int>(arcs.size())));
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  std::unordered_set<std::string_view> arc_starts;
  for (const auto &[arc_cell, arc_net] : arcs) {
    arc_starts.insert(arc_cell->from_pin);
  }

  std::vector<std::map<std::tuple<std::string, bool, std::string, bool>,
                       nlohmann::json>>
      thread_buffers(num_threads);
  size_t chunk_size_arc = (arcs.size() + num_threads - 1) / num_threads;

  for (unsigned int t = 0; t < num_threads; ++t) {
    size_t begin_idx = t * chunk_size_arc;
    size_t end_idx = std::min(begin_idx + chunk_size_arc, arcs.size());

    if (begin_idx >= arcs.size()) break;

    threads.emplace_back(&pair_analyser_graph::process_arc_segment, this, t,
                         begin_idx, end_idx, std::ref(arcs), std::ref(rpt_pair),
                         std::ref(csv_pin_db_key), std::ref(csv_pin_db_value),
                         std::ref(thread_buffers));
  }

  // Wait for all threads
  for (auto &th : threads) {
    if (th.joinable()) {
      th.join();
    }
  }

  // process_arc_segment(0, 0, arcs.size(), arcs, rpt_pair, csv_pin_db_key,
  //                     csv_pin_db_value, thread_buffers);

  // Merge results
  for (auto &buffer : thread_buffers) {
    arcs_buffer.insert(buffer.begin(), buffer.end());
  }
  for (auto [name, time] : _sparse_graph_ptrs[rpt_pair[1]]->timing_stats) {
    fmt::print("Timing stats {}: {} s\n", name, time / 1e6);
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
  fmt::print("Wrote {} arc match result to {}\n", arc_node.size(), cmp_name);
}
