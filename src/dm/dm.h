#pragma once
#include <memory>
#include <string>
#include <vector>

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
  bool is_input;
  std::pair<double, double> location;
  std::shared_ptr<Net> net;
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
  // std::vector<std::shared_ptr<Net>> nets;
  // std::vector<std::shared_ptr<Pin>> pins;
  std::vector<std::shared_ptr<Path>> paths;
  std::string tool;
};
