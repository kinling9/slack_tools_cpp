#pragma once
#include <absl/container/flat_hash_map.h>

#include <memory>

#include "analyser/analyser.h"
#include "dm/dm.h"

class flow_control {
 public:
  flow_control(std::string yml) : _yml(yml) {}
  void run();
  void parse_yml(std::string yml_file);
  std::shared_ptr<basedb> parse_rpt(std::string rpt_file, std::string rpt_type);
  void analyse(std::string mode);

 private:
  std::string _yml;
  configs _configs;
  std::shared_ptr<analyser> _analyser;
  absl::flat_hash_map<std::string, std::vector<std::shared_ptr<basedb>>> _dbs;
  absl::flat_hash_map<std::string,
                      std::vector<std::pair<std::string, std::string>>>
      _rpts;
};
