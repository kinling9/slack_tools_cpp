#pragma once
#include <map>
#include <string>

#include "csv2/reader.hpp"
#include "dm/dm.h"

// Define the full templated type for csv2::Reader
using CsvReaderType =
    csv2::Reader<csv2::delimiter<','>, csv2::quote_character<'"'>,
                 csv2::first_row_is_header<true>,
                 csv2::trim_policy::trim_whitespace>;

enum class csv_type {
  CellArc,
  NetArc,
  NetArcFanout,
  PinAT,
};

class csv_parser {
 public:
  csv_parser(){};
  void set_max_paths(std::size_t max_paths) { _max_paths = max_paths; }
  bool parse_file(csv_type type, const std::string &filename);
  void parse(csv_type type, CsvReaderType &ifs,
             const std::map<std::string, int> &header_map);

  const basedb &get_db() const { return _db; }

 public:
  basedb _db;
  std::size_t _max_paths = 0;
};
