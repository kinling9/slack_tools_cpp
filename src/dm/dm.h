#pragma once
#include <absl/container/flat_hash_map.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
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
  std::optional<std::string> cell;
  std::optional<std::string> instance;  // invs only
  double trans;
  std::optional<double> incr_delay;
  double path_delay;
  std::optional<double> pta_buf;
  std::optional<double> pta_net;
  std::optional<bool> rise_fall;
  bool is_input;  // cell input
  std::pair<double, double> location;
  std::optional<std::shared_ptr<Net>> net;
  std::optional<double> cap;         // max capacitance of the pin
  std::optional<double> path_slack;  // slack of the path

  // TODO: remove type, using db pointer instead
  std::string type;

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

 public:
  nlohmann::json to_json();
};

class Arc {
 public:
  std::string type;             // Arc类型（如：setup, hold, recovery等）
  std::string from_pin;         // 起始Pin
  std::string to_pin;           // 结束Pin
  std::array<double, 2> delay;  // delay_rise, delay_fall, according to end_pin
  // std::vector<std::shared_ptr<Path>> paths;  // 路径列表
  std::optional<int> fanout;  // fanout of the net_arc

 public:
  // nlohmann::json to_json();  // 转换为JSON格式
};

class Path {
 public:
  std::string startpoint;
  std::string endpoint;
  std::string group;
  std::string clock;
  double slack;
  std::unordered_map<std::string, double> path_params;
  std::vector<std::shared_ptr<Pin>> path;
  std::optional<double> length;
  std::optional<double> detour;
  std::optional<double> cell_delay_pct;
  std::optional<double> net_delay_pct;

 public:
  double get_length();
  double get_detour();
  double get_cell_delay_pct();
  double get_net_delay_pct();

 private:
  std::optional<double> total_delay;

 private:
  double get_delay();
};

class basedb {
 public:
  void update_loc_from_map(
      const absl::flat_hash_map<std::string, std::pair<double, double>>&
          loc_map);
  void add_arc(bool is_cell_arc, const std::shared_ptr<Arc>& arc) {
    if (is_cell_arc) {
      cell_arcs_rev[arc->to_pin][arc->from_pin] = arc;
      cell_arcs[arc->from_pin][arc->to_pin] = arc;
    } else {
      net_arcs[arc->from_pin][arc->to_pin] = arc;
    }
    all_arcs.push_back(arc);
  }
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<Arc>>>
  get_cell_arcs() {
    return cell_arcs;
  }
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<Arc>>>
  get_cell_arcs_rev() {
    return cell_arcs_rev;
  }

  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<Arc>>>
  get_net_arcs() {
    return net_arcs;
  }

 public:
  std::vector<std::shared_ptr<Path>> paths;

  // csv attributes
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<Arc>>>
      cell_arcs;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<Arc>>>
      cell_arcs_rev;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<Arc>>>
      net_arcs;
  std::unordered_map<std::string, std::shared_ptr<Pin>> pins;
  std::vector<std::shared_ptr<Arc>> all_arcs;

  std::string type;
  std::string design;
  absl::flat_hash_map<std::string, std::string> type_map;
};

namespace dm {
static const std::unordered_set<std::string> path_param = {
    "data_latency",         "clock_latency",         "clock_uncertainty",
    "input_external_delay", "output_external_delay", "library_setup_time",
};
static const std::unordered_map<std::string, bool> path_param_leda = {
    {"data_latency", true},           {"clock_latency", false},
    {"clock_uncertainty", false},     {"input_external_delay", true},
    {"output_external_delay", false}, {"library_setup_time", false},
};
static const std::unordered_map<std::string, std::string> path_param_invs = {
    {"Other End Arrival Time", "data_latency"},
    {"Uncertainty", "clock_uncertainty"},
    {"Setup", "library_setup_time"},
};

static const std::unordered_set<std::string> path_param_invs_reverse = {
    "data_latency"};
}  // namespace dm
