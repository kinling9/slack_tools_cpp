#pragma once
#include <absl/container/flat_hash_map.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "re2/re2.h"
#include "yaml-cpp/yaml.h"

class Net;
class Pin;
class Path;

class Pin {
 public:
  std::string name;
  std::string cell;
  double trans;
  double incr_delay;
  double path_delay;
  bool rise_fall;
  bool is_input;  // cell input
  std::pair<double, double> location;
  std::shared_ptr<Net> net;

 public:
  YAML::Node to_yaml();
  nlohmann::json to_json();
};

class Net {
 public:
  std::string name;
  int fanout;
  double cap;
  std::pair<std::shared_ptr<Pin>, std::shared_ptr<Pin>> pins;
};

class Path {
 public:
  std::string startpoint;
  std::string endpoint;
  std::string group;
  std::string clock;
  double slack;
  std::vector<std::shared_ptr<Pin>> path;
};

class basedb {
 public:
  void update_loc_from_map(
      const absl::flat_hash_map<std::string, std::pair<double, double>>&
          loc_map);

 public:
  // std::vector<std::shared_ptr<Net>> nets;
  // std::vector<std::shared_ptr<Pin>> pins;
  std::vector<std::shared_ptr<Path>> paths;
  std::string type;
  std::string design;
  absl::flat_hash_map<std::string, std::string> type_map;
};
