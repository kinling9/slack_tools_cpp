#include <fmt/core.h>

#include "analyser/existence_checker.h"
#include "re2/re2.h"

void existence_checker::check_existence(
    const absl::flat_hash_map<std::string, std::string> &cell_maps,
    const std::shared_ptr<basedb> &db) {
  std::filesystem::path output_dir = _configs.output_dir;
  auto check_file = output_dir / "invalid_pins.txt";
  std::filesystem::create_directories(output_dir);
  auto fmt_file = std::fopen(check_file.c_str(), "w");
  for (const auto &path : db->paths) {
    for (const auto &pin : path->path) {
      std::string cell_name;
      if (!RE2::FullMatch(pin->name, _pin_pattern, &cell_name)) {
        // TODO: using debug log
        // fmt::print("Pin: {} does not match the pattern\n", pin->name);
        continue;
      }
      if (cell_maps.find(cell_name) != cell_maps.end()) {
        if (cell_maps.at(cell_name) != pin->cell) {
          fmt::print(fmt_file, "Pin: {}, Cell: {}, Expected: {}\n", pin->name,
                     pin->cell, cell_maps.at(cell_name));
        }
      }
    }
  }
}
