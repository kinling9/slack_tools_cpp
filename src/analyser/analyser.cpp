#include "analyser/analyser.h"

#include <absl/strings/match.h>
#include <fmt/color.h>
#include <fmt/ranges.h>

#include <filesystem>

#include "utils/design_cons.h"

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
  absl::flat_hash_set<std::string> exist_rpts;
  absl::flat_hash_map<std::string, std::string> rpt_design_map;
  design_cons &cons = design_cons::get_instance();
  for (const auto &rpt : rpts) {
    std::string key = rpt.first.as<std::string>();
    std::string file_path = rpt.second["path"].as<std::string>();
    std::string name = cons.get_name(file_path);
    if (absl::StrContains(rpt.second["type"].as<std::string>(), "def")) {
      if (!rpt.second["def"]) {
        fmt::print(fmt::fg(fmt::color::red),
                   "Def is not defined in rpt {} with type {}, skip.\n", key,
                   rpt.second["type"].as<std::string>());
        continue;
      }
      std::string def = rpt.second["def"].as<std::string>();
      if (!check_file_exists(def)) {
        continue;
      }
      std::string def_name = cons.get_name(def);
      if (name != def_name) {
        fmt::print(fmt::fg(fmt::color::red),
                   "For rpt {}, design names are not the same: {} {}\n", key,
                   name, def_name);
        continue;
      }
    }
    if (check_file_exists(file_path)) {
      exist_rpts.insert(key);
      rpt_design_map[key] = name;
    }
  }
  absl::flat_hash_set<std::string> valid_rpts;
  for (const auto &rpt_tuple : _configs["analyse_tuples"]) {
    auto rpt_vec = rpt_tuple.as<std::vector<std::string>>();
    if (rpt_vec.empty()) {
      continue;
    }

    if (rpt_vec.size() != _num_rpts) {
      fmt::print(fmt::fg(fmt::color::red), "Invalid rpt_vec tuple: {}\n",
                 fmt::join(rpt_vec, ", "));
      continue;
    }
    if (!std::all_of(
            rpt_vec.begin(), rpt_vec.end(),
            [&](const std::string &rpt) { return exist_rpts.contains(rpt); })) {
      continue;
    }
    const auto &design_name = rpt_design_map[rpt_vec[0]];
    if (!std::all_of(rpt_vec.begin(), rpt_vec.end(),
                     [&](const std::string &rpt) {
                       return rpt_design_map[rpt] == design_name;
                     })) {
      fmt::print(fmt::fg(fmt::color::red),
                 "Design names are not the same: {}\n",
                 fmt::join(rpt_vec, ", "));
      continue;
    }
    std::ranges::for_each(
        rpt_vec, [&](const std::string &rpt) { valid_rpts.insert(rpt); });
    _analyse_tuples.push_back(rpt_vec);
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
