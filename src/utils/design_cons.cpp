#include <filesystem>
#include <iostream>

#include "absl/strings/match.h"
#include "utils/design_cons.h"
#include "yaml-cpp/yaml.h"

// Initialize the static member
design_cons& design_cons::get_instance() {
  static design_cons
      instance;  // Guaranteed to be destroyed and instantiated on first use
  return instance;
}

design_cons::design_cons() {
  YAML::Node periods;
  try {
    periods = YAML::LoadFile("yml/design_period.yml");
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::exit(1);
  }
  for (const auto& period : periods) {
    _period[period.first.as<std::string>()] = period.second.as<double>();
  }
}

design_cons::~design_cons() {}

double design_cons::get_period(const std::string& design_name) {
  if (_period.find(design_name) != _period.end()) {
    return _period[design_name];
  } else {
    std::cout << "The period of " << design_name << " is not found\n";
    return 0;
  }
}

std::string design_cons::get_name(const std::string& rpt_name) {
  std::filesystem::path abs_path = std::filesystem::absolute(rpt_name);
  for (const auto& [design, _] : _period) {
    if (absl::StrContains(abs_path.string(), design)) {
      return design;
    }
  }
  return "";
}
