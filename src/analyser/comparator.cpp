#include "analyser/comparator.h"

#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/match.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <ranges>

#include "utils/design_cons.h"
#include "utils/double_filter/double_filter.h"
#include "utils/double_filter/filter_machine.h"
#include "utils/utils.h"

bool comparator::parse_configs() {
  bool valid = analyser::parse_configs();
  collect_from_node("enable_mbff", _enable_mbff);
  collect_from_node("compare_mode", _compare_mode);
  if (!_valid_cmp_modes.contains(_compare_mode)) {
    fmt::print(fmt::fg(fmt::rgb(255, 0, 0)), "Invalid compare mode: {}\n",
               _compare_mode);
    valid = false;
  }
  _writer = std::make_unique<csv_writer>(
      fmt::format("{}.csv", _configs["compare_mode"].as<std::string>()));
  _writer->set_output_dir(_output_dir);
  collect_from_node("match_paths", _match_paths);
  collect_from_node("slack_margins", _slack_margins);
  collect_from_node("match_percentages", _match_percentages);
  std::string slack_filter;
  collect_from_node("slack_filter", slack_filter);
  compile_double_filter(slack_filter, _slack_filter_op_code);
  std::string diff_filter;
  collect_from_node("diff_filter", diff_filter);
  compile_double_filter(diff_filter, _diff_filter_op_code);
  return valid;
}

absl::flat_hash_set<std::string> comparator::check_valid(YAML::Node &rpts) {
  absl::flat_hash_set<std::string> exist_rpts = analyser::check_valid(rpts);
  absl::flat_hash_set<std::string> valid_rpts;
  std::vector<std::vector<std::string>> analyse_tuples;
  for (const auto &rpt_vec : _analyse_tuples) {
    if (_compare_mode != "endpoint" &&
        std::any_of(rpt_vec.begin(), rpt_vec.end(),
                    [&](const std::string &rpt) {
                      return absl::StrContains(
                          rpts[rpt]["type"].as<std::string>(), "endpoint");
                    })) {
      fmt::print(fmt::fg(fmt::color::red),
                 "Endpoint rpt is not allowed in non-endpoint comparison: {}\n",
                 fmt::join(rpt_vec, ","));
      continue;
    }
    std::ranges::for_each(
        rpt_vec, [&](const std::string &rpt) { valid_rpts.insert(rpt); });
    analyse_tuples.push_back(rpt_vec);
    if (_compare_mode == "endpoint") {
      for (const auto &rpt : rpt_vec) {
        rpts[rpt]["ignore_path"] = true;
      }
    }
  }
  _analyse_tuples = analyse_tuples;
  return valid_rpts;
}

void comparator::match(
    const std::string &cmp_name,
    const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        &path_maps,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  std::vector<int> nvps(2, 0);
  std::vector<std::size_t> path_nums(2, 0);
  design_cons &cons = design_cons::get_instance();
  auto period = cons.get_period(dbs[0]->design);
  absl::flat_hash_map<std::string, std::string> row;

  // TODO: filter only once to acc
  for (int i = 0; i < 2; i++) {
    // path_nums[i] = std::min(dbs[i]->paths.size(), _match_paths);
    for (const auto &path :
         dbs[i]->paths | std::views::take(_match_paths) |
             std::views::filter([&](const std::shared_ptr<Path> &path) {
               return double_filter(_slack_filter_op_code, path->slack);
             })) {
      ++path_nums[i];
      if (path->slack < 0) {
        ++nvps[i];
      }
    }
  }

  std::vector<double> slack_diffs;
  slack_diffs.reserve(dbs[0]->paths.size());
  std::vector<int> diff_nums(_slack_margins.size(), 0);
  absl::flat_hash_set<std::shared_ptr<Path>> path_set;
  std::vector<std::shared_ptr<writer>> writers(2);
  for (int i = 0; i < 2; i++) {
    std::string scatter_file = fmt::format("{}_scatter_{}.txt", cmp_name, i);
    if (_match_paths != std::numeric_limits<std::size_t>::max()) {
      scatter_file =
          fmt::format("{}_scatter_{}_{}.txt", cmp_name, _match_paths, i);
    }
    writers[i] = std::make_shared<writer>(scatter_file);
    writers[i]->set_output_dir(_output_dir);
    writers[i]->open();
  }
  for (const auto &[key, path] : path_maps[0]) {
    if (path_maps[1].contains(key) && !path_set.contains(path)) {
      path_set.emplace(path);
      auto diff_slack = path->slack - path_maps[1].at(key)->slack;
      if (_diff_filter_op_code.empty() ||
          double_filter(_diff_filter_op_code, diff_slack)) {
        fmt::print(writers[0]->out_file, "{} {}\n", key,
                   path_maps[0].at(key)->slack);
        fmt::print(writers[1]->out_file, "{} {}\n", key,
                   path_maps[1].at(key)->slack);
      }
      slack_diffs.push_back(diff_slack);
      for (std::size_t i = 0; i < _slack_margins.size(); i++) {
        if (abs(diff_slack) < _slack_margins[i] * period) {
          diff_nums[i]++;
        }
      }
    }
  }

  // write missing endpoints
  if (_compare_mode == "endpoint" && path_set.size() < dbs[0]->paths.size() &&
      _match_paths == std::numeric_limits<std::size_t>::max()) {
    writer endpoint_writer(
        fmt::format("{}_missing_endpoint.txt", cmp_name, _match_paths));
    endpoint_writer.set_output_dir(_output_dir);
    endpoint_writer.open();
    for (const auto &path : dbs[0]->paths) {
      if (!path_set.contains(path)) {
        fmt::print(endpoint_writer.out_file, "endpoint {} is miss.\n",
                   path->endpoint);
      }
    }
  }

  std::vector<double> diff_ratios(_slack_margins.size(), 0.0);
  for (std::size_t i = 0; i < _slack_margins.size(); i++) {
    diff_ratios[i] = static_cast<double>(diff_nums[i]) / path_nums[0];
  }

  int mismatch = path_nums[0] - slack_diffs.size();
  double average_slack_diff =
      std::accumulate(slack_diffs.begin(), slack_diffs.end(), 0.0) /
      slack_diffs.size();
  double variance_slack_diff =
      standardDeviation(slack_diffs, slack_diffs.size());

  std::vector<double> match_ratios(_match_percentages.size(), 0.0);
  std::vector<std::size_t> analyse_paths = {
      std::min(_match_paths, path_nums[0]),
      std::min(_match_paths, path_nums[1])};
  for (std::size_t i = 0; i < _match_percentages.size(); i++) {
    double percentage = _match_percentages[i];
    std::vector<absl::flat_hash_set<std::string>> percent_sets(2);
    std::vector<std::size_t> path_nums;
    std::ranges::transform(
        analyse_paths, std::back_inserter(path_nums),
        [&](std::size_t path_num) {
          return static_cast<std::size_t>(std::ceil(path_num * percentage));
        });
    for (int j = 0; j < 2; j++) {
      absl::flat_hash_map<std::string, std::shared_ptr<Path>> path_map;
      gen_map(dbs[j]->type,
              dbs[j]->paths |
                  std::views::filter([&](const std::shared_ptr<Path> &path) {
                    return double_filter(_slack_filter_op_code, path->slack);
                  }) |
                  std::views::take(path_nums[j]),
              path_map);
      for (const auto &key : path_map | std::views::keys) {
        percent_sets[j].insert(key);
      }
    }
    absl::flat_hash_set<std::string> intersection;
    for (const auto &endpoint : percent_sets[0]) {
      if (percent_sets[1].contains(endpoint)) {
        intersection.insert(endpoint);
      }
    }
    match_ratios[i] = static_cast<double>(intersection.size()) /
                      std::min(path_nums[0], path_nums[1]);
  }

  row["Design"] = dbs[0]->design;
  row["Cmp name"] = cmp_name;
  for (int i = 0; i < 2; i++) {
    row[fmt::format("Path num {}", i + 1)] = std::to_string(path_nums[i]);
    row[fmt::format("NVP{}", i + 1)] = std::to_string(nvps[i]);
  }
  row["Not found"] = std::to_string(mismatch);
  row["Avg diff"] = std::to_string(average_slack_diff);
  row["Var diff"] = std::to_string(variance_slack_diff);
  for (std::size_t i = 0; i < _slack_margins.size(); i++) {
    row[fmt::format("Diff < {}", _slack_margins[i])] =
        std::to_string(diff_ratios[i]);
  }
  for (std::size_t i = 0; i < _match_percentages.size(); i++) {
    row[fmt::format("Match {}%", _match_percentages[i] * 100)] =
        std::to_string(match_ratios[i]);
  }
  _writer->add_row(row);
}

void comparator::gen_map(
    const std::string &type, std::ranges::input_range auto &&paths,
    absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map) {
  std::function<std::vector<std::string>(std::shared_ptr<Path>)> key_generator;
  if (_compare_mode == "endpoint") {
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      return _mbff.get_ff_names(type, path->endpoint);
    };
  } else if (_compare_mode == "startpoint") {
    fmt::print(fmt::fg(fmt::color::yellow),
               "Warning: startpoint comparison will lose paths with the same "
               "startpoint!\n");
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      return {path->startpoint};
    };
  } else if (_compare_mode == "full_path") {
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      std::vector<std::string> full_path(path->path.size() - 1);
      std::transform(path->path.begin(), std::prev(path->path.end(), 1),
                     full_path.begin(),
                     [](const std::shared_ptr<Pin> &pin) { return pin->name; });
      // auto output = fmt::format("{}", fmt::join(full_path, "->"));
      auto last_ff = _mbff.get_ff_names(type, path->path.back()->name);
      std::vector<std::string> output_vec;
      for (const auto &ff : last_ff) {
        output_vec.push_back(
            fmt::format("{}->{}", fmt::join(full_path, "->"), ff));
      }
      return output_vec;
    };
  } else if (_compare_mode == "start_end") {
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      auto last_ff = _mbff.get_ff_names(type, path->endpoint);
      std::vector<std::string> output_vec;
      for (const auto &ff : last_ff) {
        output_vec.push_back(fmt::format("{}-{}", path->startpoint, ff));
      }
      return output_vec;
    };
  }
  std::ranges::for_each(paths, [&](const std::shared_ptr<Path> &path) {
    auto keys = key_generator(path);
    for (const auto &key : keys) {
      path_map[key] = path;
    }
  });
}

void comparator::analyse() {
  if (_enable_mbff) {
    _mbff.load_pattern("yml/mbff_pattern.yml");
  }
  gen_headers();
  for (const auto &rpt_pair : _analyse_tuples) {
    std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        path_maps;
    std::ranges::transform(
        rpt_pair, std::back_inserter(path_maps), [&](const auto &rpt) {
          absl::flat_hash_map<std::string, std::shared_ptr<Path>> path_map;
          gen_map(
              _dbs[rpt]->type,
              _dbs[rpt]->paths |
                  std::views::filter([&](const std::shared_ptr<Path> &path) {
                    return double_filter(_slack_filter_op_code, path->slack);
                  }) |
                  std::views::take(_match_paths),
              path_map);
          return path_map;
        });
    std::vector<std::shared_ptr<basedb>> dbs;
    std::ranges::transform(rpt_pair, std::back_inserter(dbs),
                           [&](const auto &rpt) { return _dbs[rpt]; });
    match(fmt::format("{}", fmt::join(rpt_pair, "-")), path_maps, dbs);
  }
  _writer->write();
}

void comparator::gen_headers() {
  // output headers
  _headers = {"Design", "Cmp name",  "Path num 1", "Path num 2", "NVP1",
              "NVP2",   "Not found", "Avg diff",   "Var diff"};
  for (const auto &margin : _slack_margins) {
    _headers.push_back(fmt::format("Diff < {}", margin));
  }
  for (const auto &percentage : _match_percentages) {
    _headers.push_back(fmt::format("Match {}%", percentage * 100));
  }
  _writer->set_headers(_headers);
}
