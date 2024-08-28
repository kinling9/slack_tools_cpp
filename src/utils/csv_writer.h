#pragma once
#include <absl/container/flat_hash_map.h>

#include <string>
#include <vector>

class csv_writer {
 public:
  csv_writer(const std::string &filename) : _filename(filename) {}
  csv_writer(const std::string &filename,
             const std::vector<std::string> &headers)
      : _filename(filename), _headers(headers) {
    _length = headers.size();
  }
  void set_headers(const std::vector<std::string> &headers);
  void set_output_dir(const std::string &output_dir) {
    _output_dir = output_dir;
  }
  void add_row(const std::vector<std::string> &row);
  void add_row(const absl::flat_hash_map<std::string, std::string> &row);
  void write();

 private:
  std::string _filename;
  std::string _output_dir = "output";
  std::vector<std::string> _headers;
  std::vector<std::vector<std::string>> _rows;
  std::size_t _length = 0;
};
