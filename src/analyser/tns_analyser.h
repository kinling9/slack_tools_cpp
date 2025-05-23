#pragma once
#include "arc_analyser.h"

class tns_analyser : public arc_analyser {
 public:
  tns_analyser(const YAML::Node &configs) : arc_analyser(configs){};
  void match(const std::string &cmp_name,
             const absl::flat_hash_map<std::pair<std::string_view, bool>,
                                       std::shared_ptr<Path>> &pin_map,
             const std::vector<std::shared_ptr<basedb>> &dbs) override;

 private:
  void gen_value_map() override;
  bool drop_filter(const std::tuple<std::shared_ptr<Pin>, std::shared_ptr<Pin>,
                                    std::shared_ptr<Pin>> &pin_ptr_tuple) const;
  void calculate_tns_contribution(const std::shared_ptr<basedb> &db,
                                  std::string rpt_name) const;
  void open_writers() override;

 private:
  absl::flat_hash_map<std::string, std::shared_ptr<writer>> _tns_writers;
};
