#include <fmt/core.h>
#include <fmt/ranges.h>

#include <argparse/argparse.hpp>
#include <chrono>
#include <iostream>

#include "absl/container/flat_hash_set.h"
#include "analyser/comparator.h"
#include "flow/flow_control.h"
#include "parser/leda_rpt.h"
#include "utils/design_cons.h"
#include "utils/utils.h"
#include "yaml-cpp/yaml.h"

int main(int argc, char** argv) {
  argparse::ArgumentParser program("cpp_timing_analyser");
  program.add_argument("yml").help("yml config file");

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  flow_control main_flow(program.get<std::string>("yml"));
  run_function("main_flow", [&]() { main_flow.run(); });
  return 0;
}
