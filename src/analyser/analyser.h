#pragma once
#include <absl/container/flat_hash_set.h>
#include <fmt/core.h>

#include "flow/configs.h"
#include "utils/csv_writer.h"
#include "yaml-cpp/yaml.h"

class analyser {
 public:
  analyser(const YAML::Node &configs) : _configs(configs) {}
  virtual ~analyser() = 0;
  virtual void analyse() = 0;
  virtual absl::flat_hash_set<std::string> check_valid(YAML::Node &rpts) = 0;
  virtual bool parse_configs() = 0;

 protected:
  YAML::Node _configs;
};
