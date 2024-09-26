#pragma once
#include <absl/container/flat_hash_map.h>

#include <memory>

#include "dm/dm.h"
#include "flow/configs.h"
#include "yaml-cpp/yaml.h"

class flow_control {
 public:
  flow_control(std::string yml) : _yml(yml) {}
  void run();
  void parse_yml(std::string yml_file);
  std::shared_ptr<basedb> parse_rpt(std::string rpt_file, std::string rpt_type);
  void parse();
  void analyse();
  void parse_rpt_config(YAML::Node& config);

 private:
  std::string _yml;
  configs _configs;
  // compare mode
  absl::flat_hash_map<std::string, std::vector<std::shared_ptr<basedb>>> _dbs;
  absl::flat_hash_map<std::string,
                      std::vector<std::pair<std::string, std::string>>>
      _rpts;
  // def mode
  std::vector<std::vector<std::string>> _rpt_defs;
  std::string _rpt_tool;
};
