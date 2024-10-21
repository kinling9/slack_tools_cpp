#include "analyser/fanout_analyser.h"

#include <fmt/ranges.h>

#include "utils/double_filter/double_filter.h"
#include "utils/double_filter/filter_machine.h"

absl::flat_hash_set<std::string> fanout_analyser::check_valid(
    YAML::Node &rpts) {
  absl::flat_hash_set<std::string> exist_rpts = analyser::check_valid(rpts);
  absl::flat_hash_set<std::string> valid_rpts;
  for (const auto &rpt_pair : _configs["analyse_tuples"]) {
    auto rpt_vec = rpt_pair.as<std::vector<std::string>>();
    if (!check_tuple_valid(rpt_vec, rpts, 1)) {
      continue;
    }
    // TODO: ugly
    if (!exist_rpts.contains(rpt_vec[0])) {
      continue;
    }
    std::ranges::for_each(
        rpt_vec, [&](const std::string &rpt) { valid_rpts.insert(rpt); });
    _analyse_tuples.push_back(rpt_vec);
  }
  return valid_rpts;
}

bool fanout_analyser::parse_configs() {
  bool valid = analyser::parse_configs();
  _writer = std::make_unique<csv_writer>("fanout_analyse.csv");
  _writer->set_output_dir(_output_dir);
  std::string slack_filter;
  collect_from_node("slack_filter", slack_filter);
  compile_double_filter(slack_filter, _slack_filter_op_code);
  std::string fanout_filter;
  collect_from_node("fanout_filter", fanout_filter);
  compile_double_filter(fanout_filter, _fanout_filter_op_code);
  return valid;
}

void fanout_analyser::open_writers() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string cmp_name = fmt::format("{}", fmt::join(rpt_pair, "-"));
    _arcs_writers[cmp_name] =
        std::make_shared<writer>(writer(fmt::format("{}.pins", cmp_name)));
    _arcs_writers[cmp_name]->set_output_dir(_output_dir);
    _arcs_writers[cmp_name]->open();
  }
  _writer->set_headers({"Cmp name", "Num paths", "Affected paths",
                        "Affected percent", "Num nets"});
}

void fanout_analyser::check_fanout(const std::shared_ptr<basedb> &db,
                                   const std::string &key) {
  std::unordered_set<std::string> nets;
  int path_count = 0;
  for (const auto &path : db->paths) {
    if (double_filter(_slack_filter_op_code, path->slack)) {
      bool affected = false;
      for (const auto &pin : path->path) {
        if (double_filter(_fanout_filter_op_code, pin->net->fanout)) {
          nets.insert(pin->net->name);
          affected = true;
        }
      }
      if (affected) {
        path_count++;
      }
    }
  }
  absl::flat_hash_map<std::string, std::string> row = {
      {"Cmp name", key},
      {"Num paths", std::to_string(db->paths.size())},
      {"Affected paths", std::to_string(path_count)},
      {"Affected percent",
       std::to_string(static_cast<double>(path_count) / db->paths.size())},
      {"Num nets", std::to_string(nets.size())}};
  _writer->add_row(row);
  for (const auto &net : nets) {
    fmt::print(_arcs_writers[key]->out_file, "{}\n", net);
  }
}

void fanout_analyser::analyse() {
  open_writers();
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string key = rpt_pair[0];
    check_fanout(_dbs[key], key);
  }
  _writer->write();
}
