#pragma once

#include <string>

class writer {
 public:
  writer(const std::string &filename) : _filename(filename) {}
  void set_output_dir(const std::string &output_dir) {
    _output_dir = output_dir;
  };
  void open();
  ~writer() {
    if (out_file) {
      std::fclose(out_file);
    }
  }
  FILE *out_file = nullptr;

 protected:
  std::string _filename;
  std::string _output_dir = "output";
};
