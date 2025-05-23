#pragma once
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "analyser.h"
#include "dm/dm.h"
#include "utils/rf_checker.h"
#include "utils/super_arc.h"
#include "utils/writer.h"

class arc_analyser : public analyser {
 public:
  arc_analyser(const YAML::Node &configs) : analyser(configs, 2){};
  ~arc_analyser() override = default;
  void analyse() override;

 protected:
  virtual void open_writers();
  void gen_pin2path_map(
      const std::shared_ptr<basedb> &db,
      absl::flat_hash_map<std::pair<std::string_view, bool>,
                          std::shared_ptr<Path>> &pin2path_map);

 private:
  virtual void gen_value_map();
  virtual void match(
      const std::string &cmp_name,
      const absl::flat_hash_map<std::pair<std::string_view, bool>,
                                std::shared_ptr<Path>> &pin_map,
      const std::vector<std::shared_ptr<basedb>> &dbs);
  bool parse_configs() override;

 protected:
  bool _enable_super_arc;
  bool _enable_rise_fall;
  absl::flat_hash_map<std::string, std::shared_ptr<writer>> _arcs_writers;
  std::vector<double> _delay_filter_op_code;
  std::vector<double> _fanout_filter_op_code;
  super_arc::super_arc_pattern _super_arc;
  rf_checker::rf_checker _rf_checker;
};
