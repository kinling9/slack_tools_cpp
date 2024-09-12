#include "writer.h"

#include <filesystem>

void writer::open() {
  std::filesystem::path dir = _output_dir;
  auto file_path = dir / _filename;
  std::filesystem::create_directories(dir);
  out_file = std::fopen(file_path.c_str(), "w");
}
