#include "analyser/analyser.h"

#include <fmt/color.h>
#include <fmt/ranges.h>

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
    if (rpt.second["type"].as<std::string>() == "leda_def") {
      if (!rpt.second["def"]) {
        fmt::print(fmt::fg(fmt::color::red),
                   "Def is not defined in rpt {} with type leda_def, skip.\n",
                   key);
        continue;
      }
      std::string def = rpt.second["def"].as<std::string>();
      if (!check_file_exists(def)) {
        continue;
      }
    }
    std::string file_path = rpt.second["path"].as<std::string>();
    if (check_file_exists(file_path)) {
      valid_rpts.insert(key);
    }
  }
  return valid_rpts;
}

bool analyser::check_file_exists(std::string &file_path) {
  if (!std::filesystem::exists(file_path)) {
    fmt::print(fmt::fg(fmt::color::red), "Output dir {} does not exist\n",
               file_path);
    return false;
  }
  return true;
}

bool analyser::check_tuple_valid(const std::vector<std::string> &rpt_vec,
                                 const YAML::Node &rpts) const {
  if (rpt_vec.size() != 2) {
    fmt::print(fmt::fg(fmt::color::red), "Invalid rpt_vec tuple: {}\n",
               fmt::join(rpt_vec, ", "));
    return false;
  }
  bool valid = true;
  for (const auto &rpt : rpt_vec) {
    if (!rpts[rpt]) {
      valid = false;
      fmt::print(fmt::fg(fmt::color::red),
                 "Rpt {} is not defined in the yml file\n", rpt);
      continue;
    }
  }
  return valid;
}
