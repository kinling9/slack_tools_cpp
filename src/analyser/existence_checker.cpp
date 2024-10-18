#include "analyser/existence_checker.h"

#include <fmt/color.h>

#include "utils/writer.h"

absl::flat_hash_set<std::string> existence_checker::check_valid(
    YAML::Node &rpts) {
  absl::flat_hash_set<std::string> exist_rpts = analyser::check_valid(rpts);
  absl::flat_hash_set<std::string> valid_rpts;
  for (const auto &rpt_pair : _configs["analyse_tuples"]) {
    auto rpt_vec = rpt_pair.as<std::vector<std::string>>();
    if (!check_tuple_valid(rpt_vec, rpts, 1)) {
      continue;
    }
    if (!exist_rpts.contains(rpt_vec[0])) {
      continue;
    }
    std::ranges::for_each(
        rpt_vec, [&](const std::string &rpt) { valid_rpts.insert(rpt); });
    _analyse_tuples.push_back(rpt_vec);
  }
  return valid_rpts;
}

void existence_checker::analyse() {
  for (const auto &rpt_pair : _analyse_tuples) {
    std::string key = rpt_pair[0];
    check_existence(_dbs[key], key);
  }
}
void existence_checker::check_existence(const std::shared_ptr<basedb> &db,
                                        const std::string &key) {
  writer invalid_writer(fmt::format("{}_invalid_pins.txt", key));
  invalid_writer.set_output_dir(_output_dir);
  invalid_writer.open();
  const auto &cell_maps = db->type_map;
  for (const auto &path : db->paths) {
    for (const auto &pin : path->path) {
      std::size_t pos = pin->name.find_last_of('/');
      if (pos == std::string::npos) {
        continue;
      }
      std::string cell_name = pin->name.substr(0, pos);
      if (cell_maps.contains(cell_name)) {
        if (cell_maps.at(cell_name) != pin->cell) {
          fmt::print(invalid_writer.out_file,
                     "Pin: {}, Cell: {}, Expected: {}\n", pin->name, pin->cell,
                     cell_maps.at(cell_name));
        }
      }
    }
  }
}
