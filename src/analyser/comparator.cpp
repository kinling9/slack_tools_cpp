#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <ranges>

#include "analyser/comparator.h"
#include "utils/design_cons.h"
#include "utils/utils.h"

void comparator::match(
    const std::string &design,
    const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        &path_maps,
    const std::vector<std::shared_ptr<basedb>> &dbs) {
  std::vector<int> nvps(2, 0);
  std::vector<int> path_nums(2, 0);
  design_cons &cons = design_cons::get_instance();
  auto period = cons.get_period(design);
  absl::flat_hash_map<std::string, std::string> row;

  for (int i = 0; i < 2; i++) {
    path_nums[i] = dbs[i]->paths.size();
    for (const auto &path : dbs[i]->paths) {
      if (path->slack < 0) {
        ++nvps[i];
      }
    }
  }

  std::vector<double> slack_diffs;
  slack_diffs.reserve(dbs[0]->paths.size());
  std::vector<int> diff_nums(_configs.slack_margins.size(), 0);
  absl::flat_hash_set<std::shared_ptr<Path>> path_set;
  for (const auto &[key, path] : path_maps[0]) {
    if (path_maps[1].find(key) != path_maps[1].end() &&
        path_set.find(path) == path_set.end()) {
      path_set.emplace(path);
      auto diff_slack = path->slack - path_maps[1].at(key)->slack;
      slack_diffs.push_back(diff_slack);
      for (std::size_t i = 0; i < _configs.slack_margins.size(); i++) {
        if (abs(diff_slack) < _configs.slack_margins[i] * period) {
          diff_nums[i]++;
        }
      }
    }
  }
  std::vector<double> diff_ratios(_configs.slack_margins.size(), 0.0);
  for (std::size_t i = 0; i < _configs.slack_margins.size(); i++) {
    diff_ratios[i] = static_cast<double>(diff_nums[i]) / path_nums[0];
  }

  int mismatch = dbs[0]->paths.size() - slack_diffs.size();
  double average_slack_diff =
      std::accumulate(slack_diffs.begin(), slack_diffs.end(), 0.0) /
      slack_diffs.size();
  double variance_slack_diff =
      standardDeviation(slack_diffs, slack_diffs.size());

  std::vector<double> match_ratios(_configs.match_percentages.size(), 0.0);
  for (std::size_t i = 0; i < _configs.match_percentages.size(); i++) {
    double percentage = _configs.match_percentages[i];
    std::vector<absl::flat_hash_set<std::string>> percent_sets(2);
    std::vector<std::size_t> path_nums = {
        static_cast<std::size_t>(dbs[0]->paths.size() * percentage),
        static_cast<std::size_t>(dbs[1]->paths.size() * percentage)};
    for (int j = 0; j < 2; j++) {
      // for (std::size_t k = 0; k < dbs[j]->paths.size() * percentage; k++) {
      //   percent_sets[j].insert(dbs[j]->paths[k]->endpoint);
      // }
      absl::flat_hash_map<std::string, std::shared_ptr<Path>> path_map;
      gen_map(dbs[j]->tool, dbs[j]->paths | std::views::take(path_nums[j]),
              path_map);
      for (const auto &key : path_map | std::views::keys) {
        percent_sets[j].insert(key);
      }
    }
    absl::flat_hash_set<std::string> intersection;
    for (const auto &endpoint : percent_sets[0]) {
      if (percent_sets[1].find(endpoint) != percent_sets[1].end()) {
        intersection.insert(endpoint);
      }
    }
    match_ratios[i] = static_cast<double>(intersection.size()) /
                      std::min(path_nums[0], path_nums[1]);
  }

  row["Design"] = design;
  for (int i = 0; i < 2; i++) {
    row[fmt::format("Path num {}", i + 1)] = std::to_string(path_nums[i]);
    row[fmt::format("NVP{}", i + 1)] = std::to_string(nvps[i]);
  }
  row["Not found"] = std::to_string(mismatch);
  row["Avg diff"] = std::to_string(average_slack_diff);
  row["Var diff"] = std::to_string(variance_slack_diff);
  for (std::size_t i = 0; i < _configs.slack_margins.size(); i++) {
    row[fmt::format("Diff < {}", _configs.slack_margins[i])] =
        std::to_string(diff_ratios[i]);
  }
  for (std::size_t i = 0; i < _configs.match_percentages.size(); i++) {
    row[fmt::format("Match {}%", _configs.match_percentages[i] * 100)] =
        std::to_string(match_ratios[i]);
  }
  _writer.add_row(row);
}

void comparator::gen_map(
    const std::string &tool, std::ranges::input_range auto &&paths,
    absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map) {
  std::function<std::vector<std::string>(std::shared_ptr<Path>)> key_generator;
  if (_configs.compare_mode == "endpoint") {
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      return _mbff.get_ff_names(tool, path->endpoint);
    };
  } else if (_configs.compare_mode == "startpoint") {
    fmt::print(
        "Warning: startpoint comparison will lose paths with the same "
        "startpoint!\n");
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      return {path->startpoint};
    };
  } else if (_configs.compare_mode == "full_path") {
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      std::vector<std::string> full_path(path->path.size() - 1);
      std::transform(path->path.begin(), std::prev(path->path.end(), 1),
                     full_path.begin(),
                     [](const std::shared_ptr<Pin> &pin) { return pin->name; });
      auto output = fmt::format("{}", fmt::join(full_path, "->"));
      auto last_ff = _mbff.get_ff_names(tool, path->path.back()->name);
      std::vector<std::string> output_vec;
      for (const auto &ff : last_ff) {
        output_vec.push_back(fmt::format("{}->{}", output, ff));
      }
      return output_vec;
    };
  } else if (_configs.compare_mode == "start_end") {
    key_generator =
        [&](const std::shared_ptr<Path> &path) -> std::vector<std::string> {
      auto last_ff = _mbff.get_ff_names(tool, path->endpoint);
      std::vector<std::string> output_vec;
      for (const auto &ff : last_ff) {
        output_vec.push_back(fmt::format("{}->{}", path->startpoint, ff));
      }
      return output_vec;
    };
  } else {
    throw std::runtime_error("Invalid compare mode");
  }
  std::ranges::for_each(paths, [&](const std::shared_ptr<Path> &path) {
    auto keys = key_generator(path);
    for (const auto &key : keys) {
      path_map[key] = path;
    }
  });
}

void comparator::analyse() {
  _writer.set_output_dir(_configs.output_dir);
  if (_configs.enable_mbff) {
    _mbff.load_pattern("yml/mbff_pattern.yml");
  }
  gen_headers();
  for (const auto &[design, dbs] : _dbs) {
    std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        path_maps(2);
    for (int i = 0; i < 2; i++) {
      gen_map(dbs[i]->tool,
              dbs[i]->paths | std::views::take(_configs.match_paths),
              path_maps[i]);
    }
    fmt::print("finish gen map\n");
    match(design, path_maps, dbs);
  }
  _writer.write();
}

void comparator::gen_headers() {
  // output headers
  _headers = {"Design", "Path num 1", "Path num 2", "NVP1",
              "NVP2",   "Not found",  "Avg diff",   "Var diff"};
  for (const auto &margin : _configs.slack_margins) {
    _headers.push_back(fmt::format("Diff < {}", margin));
  }
  for (const auto &percentage : _configs.match_percentages) {
    _headers.push_back(fmt::format("Match {}%", percentage * 100));
  }
  _writer.set_headers(_headers);
}
