#pragma once
#include <absl/container/flat_hash_map.h>

#include "utils/writer.h"
#include <string>
#include <vector>

class csv_writer : public writer {
public:
  csv_writer(const std::string &filename) : writer(filename) {}
  csv_writer(const std::string &filename,
             const std::vector<std::string> &headers)
      : writer(filename), _headers(headers) {
    _length = headers.size();
  }
  void set_headers(const std::vector<std::string> &headers);
  void add_row(const std::vector<std::string> &row);
  void add_row(const absl::flat_hash_map<std::string, std::string> &row);
  void write();

private:
  std::vector<std::string> _headers;
  std::vector<std::vector<std::string>> _rows;
  std::size_t _length = 0;
};
