#include "utils/csv_writer.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <filesystem>

void csv_writer::set_headers(const std::vector<std::string> &headers) {
  _headers = headers;
  _length = headers.size();
}

void csv_writer::add_row(const std::vector<std::string> &row) {
  if (row.size() != _length) {
    fmt::print("Row size does not match headers size\n");
    return;
  }
  _rows.push_back(row);
}

void csv_writer::add_row(
    const absl::flat_hash_map<std::string, std::string> &row) {
  if (row.size() != _length) {
    fmt::print("Row size does not match headers size\n");
    return;
  }
  std::vector<std::string> row_vec;
  for (const auto &header : _headers) {
    row_vec.push_back(row.at(header));
  }
  _rows.push_back(row_vec);
}

void csv_writer::write() {
  std::filesystem::path output_dir = _output_dir;
  auto csv_path = output_dir / _filename;
  std::filesystem::create_directories(output_dir);
  auto out_file = std::fopen(csv_path.c_str(), "w");
  fmt::print(out_file, "{}\n", fmt::join(_headers, ","));
  for (const auto &row : _rows) {
    fmt::print(out_file, "{}\n", fmt::join(row, ","));
  }
  std::fclose(out_file);
}
