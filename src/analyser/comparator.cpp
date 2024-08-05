#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <filesystem>

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
  for (const auto &[key, path] : path_maps[0]) {
    if (path_maps[1].find(key) != path_maps[1].end()) {
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
    for (int j = 0; j < 2; j++) {
      for (std::size_t k = 0; k < dbs[j]->paths.size() * percentage; k++) {
        percent_sets[j].insert(dbs[j]->paths[k]->endpoint);
      }
    }
    absl::flat_hash_set<std::string> intersection;
    for (const auto &endpoint : percent_sets[0]) {
      if (percent_sets[1].find(endpoint) != percent_sets[1].end()) {
        intersection.insert(endpoint);
      }
    }
    match_ratios[i] = static_cast<double>(intersection.size()) /
                      std::min(percent_sets[0].size(), percent_sets[1].size());
  }

  std::filesystem::path output_dir = "output";
  auto csv_path =
      output_dir / fmt::format("{}_{}.csv", design, _configs.compare_mode);
  std::filesystem::create_directories(output_dir);
  auto out_file = std::fopen(csv_path.c_str(), "w");
  fmt::print(out_file, "{}\n", fmt::join(_headers, ","));
  fmt::print(out_file, "{},{},{},{},{},{},{},{}", design,
             fmt::join(path_nums, ","), fmt::join(nvps, ","), mismatch,
             average_slack_diff, variance_slack_diff,
             fmt::join(diff_ratios, ","), fmt::join(match_ratios, ","));
  std::fclose(out_file);
}

void comparator::gen_map(
    const std::shared_ptr<basedb> &db,
    absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map) {
  std::function<std::string(std::shared_ptr<Path>)> key_generator;
  if (_configs.compare_mode == "endpoint") {
    key_generator = [](const std::shared_ptr<Path> &path) {
      return path->endpoint;
    };
  } else if (_configs.compare_mode == "startpoint") {
    fmt::print(
        "Warning: startpoint comparison will lose paths with the same "
        "startpoint!\n");
    key_generator = [](const std::shared_ptr<Path> &path) {
      return path->startpoint;
    };
  } else if (_configs.compare_mode == "full_path") {
    key_generator = [](const std::shared_ptr<Path> &path) {
      std::vector<std::string> full_path(path->path.size());
      std::transform(path->path.begin(), path->path.end(), full_path.begin(),
                     [](const std::shared_ptr<Pin> &pin) { return pin->name; });
      auto output = fmt::format("{}", fmt::join(full_path, "->"));
      return output;
    };
  } else if (_configs.compare_mode == "start_end") {
    key_generator = [](const std::shared_ptr<Path> &path) {
      return fmt::format("{}->{}", path->startpoint, path->endpoint);
    };
  } else {
    throw std::runtime_error("Invalid compare mode");
  }
  for (const auto &path : db->paths) {
    path_map[key_generator(path)] = path;
  }
}

void comparator::analyse() {
  for (const auto &[design, dbs] : _dbs) {
    std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
        path_maps(2);
    for (int i = 0; i < 2; i++) {
      gen_map(dbs[i], path_maps[i]);
    }
    match(design, path_maps, dbs);
  }
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
}
