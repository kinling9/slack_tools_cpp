#include "arc_analyser_graph.h"

#include <fmt/ranges.h>

#include <algorithm>
#include <cstdlib>
#include <thread>

#include "utils/cache_result.h"
#include "utils/utils.h"

bool arc_analyser_graph::parse_configs() {
  bool valid = arc_analyser::parse_configs();
  collect_from_node("allow_unplaced_pins", _allow_unplaced_pins);
  return valid;
}

void arc_analyser_graph::analyse() {
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
    init_graph(_dbs.at(rpt_pair[1]), rpt_pair[1]);
    csv_match(rpt_pair, _dbs.at(rpt_pair[0])->pins, _dbs.at(rpt_pair[1])->pins);
  }
}

void arc_analyser_graph::init_graph(const std::shared_ptr<basedb> &db,
                                    std::string name) {
  if (db == nullptr) {
    fmt::print("DB is nullptr, skip\n");
    return;
  }
  auto rise_graph = std::make_shared<sparse_graph_shortest_path_rf>(true);
  auto fall_graph = std::make_shared<sparse_graph_shortest_path_rf>(false);

  std::thread rise_thread([&]() { rise_graph->build_graph(db->all_arcs); });
  std::thread fall_thread([&]() { fall_graph->build_graph(db->all_arcs); });
  rise_thread.join();
  fall_thread.join();

  _sparse_graph_ptrs[name] = {rise_graph, fall_graph};
  rise_graph->print_stats();
}

yyjson_mut_val *arc_analyser_graph::create_pin_node(
    yyjson_mut_doc *doc, std::string_view name_sv, const bool is_input,
    const std::array<double, 2> &incr_delays,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db,
    const bool is_topin_rise) const {
  yyjson_mut_val *node = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_strncpy(doc, node, "name", name_sv.data(),
                             name_sv.length());
  yyjson_mut_obj_add_bool(doc, node, "is_input", is_input);
  yyjson_mut_obj_add_real(doc, node, "incr_delay",
                          is_topin_rise ? incr_delays[0] : incr_delays[1]);
  yyjson_mut_obj_add_bool(doc, node, "rf", is_topin_rise);
  if (!csv_pin_db.empty()) {
    if (auto pin_it = csv_pin_db.find(std::string(name_sv));
        pin_it != csv_pin_db.end()) {
      const auto &pin = pin_it->second;
      const auto &path_delays =
          pin->path_delays.value_or(std::array<double, 2>{0., 0.});
      yyjson_mut_obj_add_real(doc, node, "path_delay",
                              is_topin_rise ? path_delays[0] : path_delays[1]);
      yyjson_mut_val *loc_arr = yyjson_mut_arr(doc);
      yyjson_mut_arr_add_real(doc, loc_arr, pin->location.first);
      yyjson_mut_arr_add_real(doc, loc_arr, pin->location.second);
      yyjson_mut_obj_add_val(doc, node, "location", loc_arr);
      const auto &trans = pin->transs.value_or(std::array<double, 2>{0., 0.});
      yyjson_mut_obj_add_real(doc, node, "trans",
                              is_topin_rise ? trans[0] : trans[1]);
      const auto &caps = pin->caps.value_or(std::array<double, 2>{0., 0.});
      yyjson_mut_obj_add_real(doc, node, "cap",
                              is_topin_rise ? caps[0] : caps[1]);
    }
  }
  return node;
}

void arc_analyser_graph::process_arc_segment(
    int t, size_t begin_idx, size_t end_idx,
    const std::vector<std::shared_ptr<Arc>> &arcs,
    const std::vector<std::string> &rpt_pair,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value,
    std::vector<std::vector<std::pair<std::string, std::string>>>
        &thread_buffers,
    const std::shared_ptr<sparse_graph_shortest_path_rf> &graph_ptr,
    bool is_topin_rise) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

  auto it = arcs.begin();
  std::advance(it, begin_idx);
  auto end_it = arcs.begin();
  std::advance(end_it, end_idx);

  for (; it != end_it; ++it) {
    const auto &arc = *it;
    const auto &pin_from = arc->from_pin;
    const auto &pin_to = arc->to_pin;

    auto connect_check = graph_ptr->query_shortest_distance(pin_from, pin_to);

    if (connect_check.distance >= 0) {
      auto arc_tuple = std::make_tuple(pin_from, false, pin_to, is_topin_rise);
      process_single_connection(t, arc, connect_check, rpt_pair, csv_pin_db_key,
                                csv_pin_db_value, arc_tuple, thread_buffers,
                                doc);
    } else {
      if (is_topin_rise) {
        fmt::print("No rise connection from {} to {}, skip all operations\n",
                   pin_from, pin_to);
      } else {
        fmt::print("No fall connection from {} to {}, skip\n", pin_from,
                   pin_to);
      }
    }
  }
  yyjson_mut_doc_free(doc);
}

void arc_analyser_graph::process_single_connection(
    int t, const std::shared_ptr<Arc> &arc, const cache_result &connect_check,
    const std::vector<std::string> &rpt_pair,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value,
    const std::tuple<std::string, bool, std::string, bool> &arc_tuple,
    std::vector<std::vector<std::pair<std::string, std::string>>>
        &thread_buffers,
    yyjson_mut_doc *doc) {
  auto &[pin_from, is_frompin_rise, pin_to, is_topin_rise] = arc_tuple;

  if (!_allow_unplaced_pins &&
      (!csv_pin_db_key.contains(pin_from) || !csv_pin_db_key.contains(pin_to) ||
       !csv_pin_db_value.contains(pin_from) ||
       !csv_pin_db_value.contains(pin_to))) {
    return;
  }

  auto from = std::make_pair(pin_from, is_frompin_rise);
  auto to = std::make_pair(pin_to, is_topin_rise);

  double total_delay = 0.;
  if (is_topin_rise) {
    total_delay = arc->delay[0];
  } else {
    total_delay = arc->delay[1];
  }

  yyjson_mut_val *node = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(
      doc, node, "type",
      arc->type == arc_type::CellArc ? "cell arc" : "net arc");
  std::string from_pin =
      fmt::format("{} {}", pin_from, is_frompin_rise ? "(rise)" : "(fall)");
  yyjson_mut_obj_add_strcpy(doc, node, "from", from_pin.c_str());
  std::string to_pin =
      fmt::format("{} {}", pin_to, is_topin_rise ? "(rise)" : "(fall)");
  yyjson_mut_obj_add_strcpy(doc, node, "to", to_pin.c_str());

  // Build key section
  yyjson_mut_val *key_obj = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, node, "key", key_obj);

  yyjson_mut_val *key_pins = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, key_obj, "pins", key_pins);

  yyjson_mut_arr_append(
      key_pins, create_pin_node(doc, std::string_view(pin_from), true, {0., 0.},
                                csv_pin_db_key, is_frompin_rise));
  yyjson_mut_arr_append(
      key_pins, create_pin_node(doc, std::string_view(pin_to), true, arc->delay,
                                csv_pin_db_key, is_topin_rise));

  yyjson_mut_obj_add_real(doc, key_obj, "delay", total_delay);

  yyjson_mut_val *value_obj = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, node, "value", value_obj);

  yyjson_mut_val *value_pins = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, value_obj, "pins", value_pins);

  yyjson_mut_obj_add_real(doc, value_obj, "delay", connect_check.distance);

  yyjson_mut_arr_append(
      value_pins, create_pin_node(doc, std::string_view(pin_from), true,
                                  {0., 0.}, csv_pin_db_value, is_frompin_rise));

  bool is_cell_arc = arc->type == arc_type::CellArc;
  auto value_db = _dbs.at(rpt_pair[1]);
  for (const auto &pin_tuple : connect_check.path | std::views::adjacent<2>) {
    const auto &[mid_from_view, mid_to_view] = pin_tuple;
    // std::string mid_from = std::string(mid_from_view);
    std::shared_ptr<Arc> &mid_arc =
        is_cell_arc
            ? value_db
                  ->cell_arcs_flat[std::make_pair(mid_from_view, mid_to_view)]
            : value_db
                  ->net_arcs_flat[std::make_pair(mid_from_view, mid_to_view)];
    is_cell_arc = !is_cell_arc;
    yyjson_mut_arr_append(
        value_pins,
        create_pin_node(doc, mid_to_view, !is_cell_arc, mid_arc->delay,
                        csv_pin_db_value, is_topin_rise));
  }

  if (arc->fanout.has_value()) {
    yyjson_mut_obj_add_int(doc, key_obj, "fanout", arc->fanout.value());
  }

  bool valid_location = true;
  if (csv_pin_db_key.contains(pin_to)) {
    double slack_val = 0;
    if (is_topin_rise) {
      slack_val = csv_pin_db_key.at(pin_to)->path_slacks.value()[0];
    } else {
      slack_val = csv_pin_db_key.at(pin_to)->path_slacks.value()[1];
    }
    yyjson_mut_obj_add_real(doc, key_obj, "slack", slack_val);

    if (!csv_pin_db_value.empty() && csv_pin_db_value.contains(pin_to)) {
      double val_slack = 0;
      if (is_topin_rise) {
        val_slack = csv_pin_db_key.at(pin_to)->path_slacks.value()[0];
      } else {
        val_slack = csv_pin_db_key.at(pin_to)->path_slacks.value()[1];
      }
      yyjson_mut_obj_add_real(doc, value_obj, "slack", val_slack);
      yyjson_mut_obj_add_real(doc, node, "delta_slack", slack_val - val_slack);
    }
  }

  std::vector<std::pair<float, float>> locs_key;
  std::vector<std::pair<float, float>> locs_value;

  auto collect_loc = [&](const std::string &p, const auto &db,
                         std::vector<std::pair<float, float>> &locs) {
    if (db.contains(p)) {
      locs.push_back(db.at(p)->location);
      return true;
    }
    return false;
  };

  if (!csv_pin_db_key.empty()) {
    if (!collect_loc(pin_from, csv_pin_db_key, locs_key))
      valid_location = false;
    if (!collect_loc(pin_to, csv_pin_db_key, locs_key)) valid_location = false;
  }

  if (valid_location && !csv_pin_db_value.empty()) {
    if (!collect_loc(pin_from, csv_pin_db_value, locs_value))
      valid_location = false;

    for (const auto &pin_tuple : connect_check.path | std::views::adjacent<2>) {
      const auto &[_, mid_to_view] = pin_tuple;
      std::string mid_to = std::string(mid_to_view);
      if (!collect_loc(mid_to, csv_pin_db_value, locs_value)) {
        valid_location = false;
        break;
      }
    }
  }

  if (valid_location) {
    double len_k = manhattan_distance(locs_key);
    double len_v = manhattan_distance(locs_value);
    yyjson_mut_obj_add_real(doc, key_obj, "length", len_k);
    yyjson_mut_obj_add_real(doc, value_obj, "length", len_v);
    yyjson_mut_obj_add_real(doc, node, "delta_length", len_k - len_v);
  }

  double delta_delay = total_delay - connect_check.distance;
  yyjson_mut_obj_add_real(doc, node, "delta_delay", delta_delay);

  // Store in thread-local buffer
  yyjson_write_err err;
  // const char *json_str =
  //     yyjson_mut_val_write_opts(node, YYJSON_WRITE_PRETTY, NULL, NULL, &err);
  const char *json_str = yyjson_mut_val_write_opts(
      node, YYJSON_WRITE_ALLOW_INVALID_UNICODE, NULL, NULL, &err);
  if (json_str) {
    std::string k = fmt::format("{} {}-{} {}", std::get<0>(arc_tuple),
                                std::get<1>(arc_tuple) ? "(rise)" : "(fall)",
                                std::get<2>(arc_tuple),
                                std::get<3>(arc_tuple) ? "(rise)" : "(fall)");
    thread_buffers[t].emplace_back(k, std::string(json_str));
    free((void *)json_str);
  } else {
    fmt::print("Failed to write JSON: {}\n", err.msg);
  }
}

void arc_analyser_graph::csv_match(
    const std::vector<std::string> &rpt_pair,
    const std::unordered_map<std::string, std::shared_ptr<Pin>> &csv_pin_db_key,
    const std::unordered_map<std::string, std::shared_ptr<Pin>>
        &csv_pin_db_value) {
  const std::vector<std::shared_ptr<Arc>> &arcs =
      _dbs.at(rpt_pair[0])->all_arcs;

  if (!_sparse_graph_ptrs.contains(rpt_pair[1])) {
    fmt::print("No graph for type {}\n", rpt_pair[1]);
    return;
  }
  auto &[rise_graph, fall_graph] = _sparse_graph_ptrs[rpt_pair[1]];

  unsigned int num_threads =
      std::max(1u, std::min(4u, static_cast<unsigned int>(arcs.size())));
  std::vector<std::thread> threads;
  threads.reserve(num_threads * 2);

  std::unordered_set<std::string_view> arc_starts;
  for (const auto &arc : arcs) {
    arc_starts.insert(arc->from_pin);
  }

  std::vector<std::vector<std::pair<std::string, std::string>>> thread_buffers(
      num_threads * 2);
  size_t chunk_size_arc = (arcs.size() + num_threads - 1) / num_threads;

  for (unsigned int t = 0; t < num_threads; ++t) {
    size_t begin_idx = t * chunk_size_arc;
    size_t end_idx = std::min(begin_idx + chunk_size_arc, arcs.size());

    if (begin_idx >= arcs.size()) break;

    threads.emplace_back(&arc_analyser_graph::process_arc_segment, this, t,
                         begin_idx, end_idx, std::ref(arcs), std::ref(rpt_pair),
                         std::ref(csv_pin_db_key), std::ref(csv_pin_db_value),
                         std::ref(thread_buffers), rise_graph, true);
  }

  for (unsigned int t = 0; t < num_threads; ++t) {
    size_t begin_idx = t * chunk_size_arc;
    size_t end_idx = std::min(begin_idx + chunk_size_arc, arcs.size());

    if (begin_idx >= arcs.size()) break;

    threads.emplace_back(&arc_analyser_graph::process_arc_segment, this,
                         t + num_threads, begin_idx, end_idx, std::ref(arcs),
                         std::ref(rpt_pair), std::ref(csv_pin_db_key),
                         std::ref(csv_pin_db_value), std::ref(thread_buffers),
                         fall_graph, false);
  }

  // Wait for all threads
  for (auto &th : threads) {
    if (th.joinable()) {
      th.join();
    }
  }

  for (auto [name, time] :
       _sparse_graph_ptrs[rpt_pair[1]].first->timing_stats) {
    fmt::print("Rise graph: Timing stats {}: {} s\n", name, time / 1e6);
  }
  for (auto [name, time] :
       _sparse_graph_ptrs[rpt_pair[1]].second->timing_stats) {
    fmt::print("Fall graph: Timing stats {}: {} s\n", name, time / 1e6);
  }

  std::vector<std::pair<std::string, std::string>> all_results;
  for (auto &buf : thread_buffers) {
    for (auto &p : buf) {
      all_results.emplace_back(std::move(p));
    }
  }

  std::sort(all_results.begin(), all_results.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
  auto &outfile = _arcs_writers[cmp_name]->out_file;

  fmt::print(outfile, "{{\n");
  for (size_t i = 0; i < all_results.size(); ++i) {
    fmt::print(outfile, "  \"{}\": {}{}\n", all_results[i].first,
               all_results[i].second, (i == all_results.size() - 1) ? "" : ",");
  }
  fmt::print(outfile, "}}");

  fmt::print("Wrote {} arc match results to {}\n", all_results.size(),
             cmp_name);
}
