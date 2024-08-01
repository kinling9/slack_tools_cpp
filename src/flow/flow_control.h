#pragma once
#include <memory>

#include "analyser/analyser.h"
#include "dm/dm.h"

class flow_control {
 public:
  flow_control(std::string yml) : _yml(yml) {}
  void run();
  void parse_yml(std::string yml_file);
  void parse_rpt(std::string rpt_file, std::string rpt_type);
  void analyse();

 private:
  std::string _yml;
  configs _configs;
  std::shared_ptr<analyser> _analyser;
  std::vector<std::shared_ptr<basedb>> _dbs;
  std::vector<std::pair<std::string, std::string>> _rpts;
};
