#pragma once
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "analyser.h"
#include "dm/dm.h"
#include "utils/writer.h"

class path_analyser : public analyser {
 public:
  path_analyser(const YAML::Node &configs) : analyser(configs, 2) {};
  ~path_analyser() override = default;

  void analyse() override;

 private:
  bool parse_configs() override;
  void open_writers();
  void check_path(const std::shared_ptr<basedb> &db, const std::string &key);
};
