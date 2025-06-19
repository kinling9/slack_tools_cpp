#pragma once
#include <csv.hpp>

#include "dm/dm.h"

class csv_parser {
 public:
  csv_parser(){};
  void set_max_paths(std::size_t max_paths) { _max_paths = max_paths; }
  bool parse_file(bool is_cell_arc, const std::string &filename);
  void parse(bool is_cell_arc, csv::CSVReader &ifs);

  const basedb &get_db() const { return _db; }

 public:
  basedb _db;
  std::size_t _max_paths = 0;
};
