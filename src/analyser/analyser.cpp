#include "analyser/analyser.h"

analyser::~analyser() {}

bool analyser::parse_configs() {
  if (!_configs["output_dir"].IsDefined()) {
    fmt::print("output_dir is not defined in configs\n");
    return false;
  }
  if (!_configs["analyse_tuples"].IsDefined()) {
    fmt::print("analyse_tuples is not defined in configs\n");
    return false;
  }
  return true;
}
