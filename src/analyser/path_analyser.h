#pragma once
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "analyser.h"
#include "dm/dm.h"
#include "utils/analyse_filter.h"
#include "utils/mbff_pattern.h"
#include "utils/writer.h"

class path_analyser : public analyser {
 public:
  path_analyser(const YAML::Node &configs) : analyser(configs, 2){};
  ~path_analyser() override = default;

  void analyse() override;

 private:
  bool parse_configs() override;
  void open_writers();
  void gen_headers();
  nlohmann::json path_analyse(const std::vector<std::shared_ptr<Path>> &paths);
  void match(
      const std::string &cmp_name, const std::string &design,
      const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
          &path_maps);
  void gen_endpoints_map(
      const std::string &type, std::ranges::input_range auto &&paths,
      absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map);

 private:
  bool _enable_mbff;
  std::unordered_map<std::string, std::shared_ptr<writer>> _arcs_writers;
  std::unordered_map<std::string, std::shared_ptr<writer>> _paths_writers;
  std::unordered_map<std::string, nlohmann::json> _paths_buffer;
  std::unordered_map<std::string, double> _paths_delay;
  absl::flat_hash_map<std::pair<std::string, std::string>, nlohmann::json>
      _arcs_buffer;
  absl::flat_hash_map<std::pair<std::string, std::string>, std::string>
      _filter_cache;
  std::unordered_set<std::string> _path_keys;
  std::vector<std::unique_ptr<analyse_filter>> _filters;
  mbff_pattern _mbff;
  std::unique_ptr<csv_writer> _writer;
};
