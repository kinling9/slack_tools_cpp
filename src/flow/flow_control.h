#pragma once
#include <absl/container/flat_hash_map.h>

#include <memory>

#include "analyser/analyser.h"
#include "dm/dm.h"
#include "flow/configs.h"
#include "yaml-cpp/yaml.h"

class flow_control {
 public:
  flow_control(std::string yml) : _yml(yml) {}
  void run();
  void parse_yml(std::string yml_file);
  void parse_rpt(const YAML::Node& rpt, std::string key);
  // void parse_rpts();
  // void analyse();
  // void parse_rpt_config(const YAML::Node& rpt);

 private:
  std::string _yml;
  configs _configs;
  // compare mode
  absl::flat_hash_map<std::string, std::shared_ptr<basedb>> _dbs;
  std::mutex _dbs_mutex;
  absl::flat_hash_map<std::string,
                      std::vector<std::pair<std::string, std::string>>>
      _rpts;
  // def mode
  std::vector<std::vector<std::string>> _rpt_defs;
  std::string _rpt_tool;
  std::unique_ptr<analyser> _analyser;
};
