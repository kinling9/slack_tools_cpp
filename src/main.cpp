#include <argparse/argparse.hpp>
#include <chrono>
#include <fstream>
#include <iostream>

#include "parser/leda_rpt.h"
#include "utils/utils.h"
#include "yaml-cpp/yaml.h"

int main(int argc, char** argv) {
  argparse::ArgumentParser program("cpp_timing_analyser");
  program.add_argument("yml").help("yml config file");

  auto start = std::chrono::high_resolution_clock::now();
  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  std::cout << program.get<std::string>("yml") << "\n";
  YAML::Node config;
  try {
    config = YAML::LoadFile(program.get<std::string>("yml"));
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  std::cout << config << "\n";
  auto mode = config["mode"].as<std::string>();
  if (mode == "compare") {
    auto types = config["type"].as<std::vector<std::string>>();
    auto rpts = config["rpts"].as<std::vector<std::string>>();
    if (rpts.size() != 2 && types.size() != 2) {
      std::cerr << "Please provide two files for comparison\n";
      std::exit(1);
    }
    if (types[0] != "leda" && types[1] != "leda") {
      std::cerr << "Only leda_rpt is supported\n";
      std::exit(1);
    }
    leda_rpt_parser parser0;
    std::cout << "Parsing " << rpts[0] << "\n";
    parser0.parse_file(rpts[0]);
    std::cout << "Parsing " << rpts[1] << "\n";
    leda_rpt_parser parser1;
    parser1.parse_file(rpts[1]);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "Elapsed time: " << elapsed.count() << " s\n";
  return 0;
}
