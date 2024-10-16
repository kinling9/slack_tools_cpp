#include "analyser/analyser.h"

#include <fmt/color.h>

#include <filesystem>

analyser::~analyser() {}

bool analyser::parse_configs() {
  if (!_configs["output_dir"]) {
    fmt::print("output_dir is not defined in configs\n");
    return false;
  }
  collect_from_node("output_dir", _output_dir);
  if (!_configs["analyse_tuples"]) {
    fmt::print("analyse_tuples is not defined in configs\n");
    return false;
  }
  return true;
}

absl::flat_hash_set<std::string> analyser::check_valid(YAML::Node &rpts) {
  absl::flat_hash_set<std::string> valid_rpts;
  for (const auto &rpt : rpts) {
    std::string key = rpt.first.as<std::string>();
    std::string file_path = rpt.second["path"].as<std::string>();
    if (std::filesystem::exists(file_path)) {
      valid_rpts.insert(key);
    } else {
      fmt::print(fmt::fg(fmt::color::red), "File {} does not exist, skip.\n",
                 file_path);
    }
  }
  return valid_rpts;
}
