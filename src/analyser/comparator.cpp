#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <filesystem>

#include "analyser/comparator.h"
#include "utils/design_cons.h"
#include "utils/utils.h"

void comparator::match() {
  // output headers
  std::vector<std::string> headers = {"Design",   "Path num 1", "Path num 2",
                                      "NVP1",     "NVP2",       "Not found",
                                      "Avg diff", "Var diff"};
  for (const auto &margin : _configs.slack_margins) {
    headers.push_back(fmt::format("Diff < {}", margin));
  }
  for (const auto &percentage : _configs.match_percentages) {
    headers.push_back(fmt::format("Match {}%", percentage * 100));
  }

  std::vector<int> nvps(2, 0);
  std::vector<int> path_nums(2, 0);
  design_cons &cons = design_cons::get_instance();
  auto period = cons.get_period(_configs.design_name);

  std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
      path_maps(2);
  if (_configs.compare_mode == "endpoint") {
    for (int i = 0; i < 2; i++) {
      path_nums[i] = _dbs[i]->paths.size();
      for (const auto &path : _dbs[i]->paths) {
        path_maps[i][path->endpoint] = path;
        if (path->slack < 0) {
          ++nvps[i];
        }
      }
    }
  }

  _slack_diffs.reserve(_dbs[0]->paths.size());
  std::vector<int> diff_nums(_slack_margins.size(), 0);
  for (const auto &[endpoint, path] : path_maps[0]) {
    if (path_maps[1].find(endpoint) != path_maps[1].end()) {
      auto diff_slack = path->slack - path_maps[1][endpoint]->slack;
      _slack_diffs.push_back(diff_slack);
      for (std::size_t i = 0; i < _slack_margins.size(); i++) {
        if (abs(diff_slack) < _slack_margins[i] * period) {
          diff_nums[i]++;
        }
      }
    }
  }
  std::vector<double> diff_ratios(_slack_margins.size(), 0.0);
  for (std::size_t i = 0; i < _slack_margins.size(); i++) {
    diff_ratios[i] = static_cast<double>(diff_nums[i]) / path_nums[0];
  }

  int mismatch = _dbs[0]->paths.size() - _slack_diffs.size();
  double average_slack_diff =
      std::accumulate(_slack_diffs.begin(), _slack_diffs.end(), 0.0) /
      _slack_diffs.size();
  double variance_slack_diff =
      standardDeviation(_slack_diffs, _slack_diffs.size());

  std::vector<double> match_ratios(_match_percentages.size(), 0.0);
  for (std::size_t i = 0; i < _match_percentages.size(); i++) {
    double percentage = _match_percentages[i];
    std::vector<absl::flat_hash_set<std::string>> percent_sets(2);
    for (int j = 0; j < 2; j++) {
      for (std::size_t k = 0; k < _dbs[j]->paths.size() * percentage; k++) {
        percent_sets[j].insert(_dbs[j]->paths[k]->endpoint);
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
  auto csv_path = output_dir / "compare.csv";
  std::filesystem::create_directories(output_dir);
  auto out_file = std::fopen(csv_path.c_str(), "w");
  fmt::print(out_file, "{}\n", fmt::join(headers, ","));
  fmt::print(out_file, "{},{},{},{},{},{},{},{}", _configs.design_name,
             fmt::join(path_nums, ","), fmt::join(nvps, ","), mismatch,
             average_slack_diff, variance_slack_diff,
             fmt::join(diff_ratios, ","), fmt::join(match_ratios, ","));
  std::fclose(out_file);
}
