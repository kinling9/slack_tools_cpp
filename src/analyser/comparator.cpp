#include <absl/container/flat_hash_map.h>

#include "analyser/comparator.h"
#include "utils/utils.h"

void comparator::match() {
  std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
      path_maps(2);
  if (_configs.compare_mode == "endpoint") {
    for (int i = 0; i < 2; i++) {
      for (const auto &path : _dbs[i]->paths) {
        path_maps[i][path->endpoint] = path;
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
        if (diff_slack < _slack_margins[i]) {
          diff_nums[i]++;
        }
      }
    }
  }
  int mismatch = _dbs[0]->paths.size() - _slack_diffs.size();
  double average_slack_diff =
      std::accumulate(_slack_diffs.begin(), _slack_diffs.end(), 0.0) /
      _slack_diffs.size();
  double variance_slack_diff =
      standardDeviation(_slack_diffs, _slack_diffs.size());
  std::cout << "Average slack difference: " << average_slack_diff << "\n";
  std::cout << "Variance of slack difference: " << variance_slack_diff << "\n";
  std::cout << "missed paths: " << mismatch << "\n";
}
