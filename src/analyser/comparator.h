#pragma once
#include <absl/container/flat_hash_map.h>

#include "analyser/analyser.h"
#include "dm/dm.h"
#include "utils/mbff_pattern.h"

class comparator : public analyser {
 public:
  comparator(const YAML::Node &configs) : analyser(configs) {};
  ~comparator() override = default;
  void match(
      const std::string &design,
      const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
          &path_maps,
      const std::vector<std::shared_ptr<basedb>> &dbs) {};
  void gen_map(
      const std::string &tool, std::ranges::input_range auto &&paths,
      absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map) {};
  void gen_headers() {};
  void analyse() override {};

  absl::flat_hash_set<std::string> check_valid(YAML::Node &rpts) override;
  bool parse_configs() override;

 private:
  std::vector<std::string> _headers;
  bool _enable_mbff = false;
  std::string _compare_mode;
  std::size_t _match_paths = std::numeric_limits<std::size_t>::max();
  std::vector<double> _slack_margins = {0.01, 0.03, 0.05, 0.1};
  std::vector<double> _match_percentages = {0.01, 0.03, 0.1, 0.5, 1};
  std::vector<double> _slack_filter_op_code;
  mbff_pattern _mbff;
};
