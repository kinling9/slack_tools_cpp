#pragma once
#include <absl/container/flat_hash_set.h>
#include <fmt/core.h>

#include <type_traits>

#include "dm/dm.h"
#include "flow/configs.h"
#include "utils/csv_writer.h"
#include "yaml-cpp/yaml.h"

class analyser {
 public:
  analyser(const YAML::Node &configs, const int num_rpts)
      : _configs(configs), _num_rpts(num_rpts) {}
  virtual ~analyser() = 0;

  void set_db(
      const absl::flat_hash_map<std::string, std::shared_ptr<basedb>> &dbs) {
    _dbs = dbs;
  }
  void collect_from_node(std::string name, auto &value) {
    if (_configs[name]) {
      using T = std::remove_reference<decltype(value)>::type;
      value = _configs[name].as<T>();
    }
  };

  virtual void analyse() = 0;
  virtual absl::flat_hash_set<std::string> check_valid(YAML::Node &rpts);
  virtual bool parse_configs();

 protected:
  YAML::Node _configs;
  absl::flat_hash_map<std::string, std::shared_ptr<basedb>> _dbs;
  std::vector<std::vector<std::string>> _analyse_tuples;
  std::string _output_dir;
  std::size_t _num_rpts;

 private:
  bool check_file_exists(std::string &file_path);
};
