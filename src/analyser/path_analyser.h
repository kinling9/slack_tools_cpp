#pragma once
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "analyser.h"
#include "dm/dm.h"
#include "utils/analyse_filter.h"
#include "utils/ignore_filter.h"
#include "utils/mbff_pattern.h"
#include "utils/super_arc.h"
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
      const std::string &cmp_name,
      const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
          &path_maps,
      const std::vector<std::shared_ptr<basedb>> &dbs);
  void gen_endpoints_map(
      const std::string &type, std::ranges::input_range auto &&paths,
      absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map);

 private:
  bool _enable_mbff;
  bool _enable_super_arc;
  bool _enable_ignore_filter;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<writer>>>
      _writers;
  std::unordered_map<std::string, nlohmann::json> _cmps_buffer;
  std::unordered_map<std::string, nlohmann::json> _paths_buffer;
  std::unordered_map<std::string, double> _cmps_delay;
  absl::flat_hash_map<std::pair<std::string, std::string>, nlohmann::json>
      _arcs_buffer;
  absl::flat_hash_map<std::pair<std::string, std::string>, std::string>
      _filter_cache;
  std::unordered_set<std::string> _path_keys;
  std::vector<std::unique_ptr<analyse_filter>> _filters;
  mbff_pattern _mbff;
  super_arc::super_arc_pattern _super_arc;
  ignore_filter::ignore_filter_pattern _ignore_filter;
  std::unique_ptr<csv_writer> _csv_writer;
};

namespace path_analyse {
static const std::unordered_set<std::string> path_param_contribute = {
    "data_latency",         "clock_latency",         "clock_uncertainty",
    "input_external_delay", "output_external_delay", "library_setup_time",
};
}
