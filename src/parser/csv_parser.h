#pragma once
#include <csv.hpp>

#include "dm/dm.h"

enum class csv_type {
  CellArc,
  NetArc,
  PinAT,
};

class csv_parser {
 public:
  csv_parser(){};
  void set_max_paths(std::size_t max_paths) { _max_paths = max_paths; }
  bool parse_file(csv_type type, const std::string &filename);
  void parse(csv_type type, csv::CSVReader &ifs);

  const basedb &get_db() const { return _db; }

 public:
  basedb _db;
  std::size_t _max_paths = 0;
};
