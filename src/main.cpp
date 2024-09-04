#include <argparse/argparse.hpp>
#include <iostream>

#include "flow/flow_control.h"
#include "parser/def_parser.h"
#include "utils/mbff_pattern.h"
#include "utils/utils.h"
#include "yaml-cpp/yaml.h"

int main(int argc, char** argv) {
  mbff_pattern mbff("yml/mbff_pattern.yml");
  return 0;
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
