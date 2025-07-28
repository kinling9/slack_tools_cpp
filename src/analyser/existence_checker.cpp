#include "analyser/existence_checker.h"

#include <fmt/color.h>

#include "utils/writer.h"

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
                     "Pin: {}, Cell: {}, Expected: {}\n", pin->name,
                     pin->cell.value_or(""), cell_maps.at(cell_name));
        }
      }
    }
  }
}
