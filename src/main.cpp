#include <fmt/core.h>
#include <fmt/ranges.h>

#include <argparse/argparse.hpp>
#include <chrono>
#include <iostream>

#include "absl/container/flat_hash_set.h"
#include "analyser/comparator.h"
#include "parser/leda_rpt.h"
#include "utils/design_cons.h"
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
    design_cons& cons = design_cons::get_instance();
    std::string design;
    absl::flat_hash_set<std::string> designs;
    for (const auto& rpt : rpts) {
      auto design_name = cons.get_name(rpt);
      if (design_name == "") {
        fmt::print(
            "The design period corresponding to rpt {} cannot be found\n", rpt);
        std::exit(1);
      }
      designs.emplace(design_name);
    }
    if (designs.size() != 1) {
      fmt::print("The reports are generated from different designs {}\n",
                 fmt::join(designs, ","));
      std::exit(1);
    }
    design = designs.begin()->data();

    leda_rpt_parser parser0;
    std::cout << "Parsing " << rpts[0] << "\n";
    parser0.parse_file(rpts[0]);
    std::cout << "Parsing " << rpts[1] << "\n";
    leda_rpt_parser parser1;
    parser1.parse_file(rpts[1]);
    configs cmp_config{config["compare_mode"].as<std::string>(), design};
    comparator cmp(cmp_config, {std::make_shared<basedb>(parser0.get_db()),
                                std::make_shared<basedb>(parser1.get_db())});
    cmp.match();
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "Elapsed time: " << elapsed.count() << " s\n";
  return 0;
}
