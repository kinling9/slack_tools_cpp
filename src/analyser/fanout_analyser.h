#pragma once
#include "analyser.h"

class fanout_analyser : public analyser {
 public:
  fanout_analyser(const YAML::Node &configs) : analyser(configs, 1){};
  ~fanout_analyser() override = default;
  void analyse() override;

 private:
  bool parse_configs() override;
  void open_writers();
  void check_fanout(const std::shared_ptr<basedb> &db, const std::string &key);

 private:
  std::unordered_map<std::string, std::shared_ptr<writer>> _arcs_writers;
  std::vector<double> _slack_filter_op_code;
  std::vector<double> _fanout_filter_op_code;
  std::unique_ptr<csv_writer> _writer;
};
