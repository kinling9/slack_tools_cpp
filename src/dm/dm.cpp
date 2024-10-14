#include "dm/dm.h"

YAML::Node Pin::to_yaml() {
  YAML::Node node;
  // pack for arc analyser
  node["name"] = name;
  node["incr_delay"] = incr_delay;
  node["path_delay"] = path_delay;
  node["location"] = YAML::Node();
  node["location"].push_back(location.first);
  node["location"].push_back(location.second);
  node.SetStyle(YAML::EmitterStyle::Flow);
  return node;
}
