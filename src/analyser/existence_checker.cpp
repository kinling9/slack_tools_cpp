#include "analyser/existence_checker.h"

#include <fmt/color.h>

#include "utils/design_cons.h"

bool existence_checker::parse_configs() {
  bool valid = analyser::parse_configs();
  return valid;
}

absl::flat_hash_set<std::string> existence_checker::check_valid(
    YAML::Node &rpts) {
  absl::flat_hash_set<std::string> exist_rpts = analyser::check_valid(rpts);
  absl::flat_hash_set<std::string> valid_rpts;
  design_cons &cons = design_cons::get_instance();
  for (const auto &rpt_pair : _configs["analyse_tuples"]) {
    auto rpt_vec = rpt_pair.as<std::vector<std::string>>();
    if (!check_tuple_valid(rpt_vec, rpts)) {
      continue;
    }
    std::string rpt_0 = rpts[rpt_vec[0]]["path"].as<std::string>();
    std::string rpt_1 = rpts[rpt_vec[1]]["path"].as<std::string>();
    if (cons.get_name(rpt_0) != cons.get_name(rpt_1)) {
      fmt::print(fmt::fg(fmt::rgb(255, 0, 0)),
                 "Design names are not the same: {} {}\n", rpt_0, rpt_1);
      continue;
    }
    if (!exist_rpts.contains(rpt_vec[0]) || !exist_rpts.contains(rpt_vec[1])) {
      continue;
    }
    std::ranges::for_each(
        rpt_vec, [&](const std::string &rpt) { valid_rpts.insert(rpt); });
    _analyse_tuples.push_back(rpt_vec);
  }
  return valid_rpts;
}
//
// #include <fmt/core.h>
//
// #include <filesystem>
//
// #include "re2/re2.h"
//
// void existence_checker::check_existence(
//     const absl::flat_hash_map<std::string, std::string> &cell_maps,
//     const std::shared_ptr<basedb> &db) {
//   std::filesystem::path output_dir = _configs.output_dir;
//   auto check_file = output_dir / "invalid_pins.txt";
//   std::filesystem::create_directories(output_dir);
//   auto fmt_file = std::fopen(check_file.c_str(), "w");
//   for (const auto &path : db->paths) {
//     for (const auto &pin : path->path) {
//       std::string cell_name;
//       if (!RE2::FullMatch(pin->name, _pin_pattern, &cell_name)) {
//         // TODO: using debug log
//         // fmt::print("Pin: {} does not match the pattern\n", pin->name);
//         continue;
//       }
//       if (cell_maps.contains(cell_name)) {
//         if (cell_maps.at(cell_name) != pin->cell) {
//           fmt::print(fmt_file, "Pin: {}, Cell: {}, Expected: {}\n", pin->name,
//                      pin->cell, cell_maps.at(cell_name));
//         }
//       }
//     }
//   }
// }
