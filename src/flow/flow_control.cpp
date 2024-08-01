#include <fmt/core.h>
#include <fmt/ranges.h>

#include "absl/container/flat_hash_set.h"
#include "analyser/comparator.h"
#include "flow/flow_control.h"
#include "parser/leda_rpt.h"
#include "utils/design_cons.h"
#include "utils/utils.h"
#include "yaml-cpp/yaml.h"

void flow_control::parse_yml(std::string yml_file) {
  YAML::Node config;
  try {
    config = YAML::LoadFile(yml_file);
  } catch (const std::exception& err) {
    throw fmt::system_error(errno, "cannot open yml file {}, {}", yml_file,
                            err.what());
    std::exit(1);
  }
  fmt::print("Parsing {}\n", yml_file);
  std::cout << config << "\n";
  auto mode = config["mode"].as<std::string>();
  if (mode == "compare") {
    auto types = config["type"].as<std::vector<std::string>>();
    auto rpts = config["rpts"].as<std::vector<std::string>>();
    if (rpts.size() != 2 && types.size() != 2) {
      throw fmt::system_error(errno, "Please provide two files for comparison");
      std::exit(1);
    }
    if (types[0] != "leda" && types[1] != "leda") {
      throw fmt::system_error(errno, "Currently, only leda_rpt is supported");
      std::exit(1);
    }
    design_cons& cons = design_cons::get_instance();
    std::string design;
    absl::flat_hash_set<std::string> designs;
    for (std::size_t i = 0; i < rpts.size(); i++) {
      auto rpt = rpts[i];
      auto design_name = cons.get_name(rpt);
      if (design_name == "") {
        fmt::print(
            "The design period corresponding to rpt {} cannot be found\n", rpt);
        std::exit(1);
      }
      designs.emplace(design_name);
      _rpts.emplace_back(rpt, types[i]);
    }
    if (designs.size() != 1) {
      fmt::print("The reports are generated from different designs {}\n",
                 fmt::join(designs, ","));
      std::exit(1);
    }
    design = designs.begin()->data();
    _configs = configs{config["compare_mode"].as<std::string>(), design};
  }
}

void flow_control::parse_rpt(std::string rpt_file, std::string rpt_type) {
  if (rpt_type == "leda") {
    leda_rpt_parser parser;
    fmt::print("Parsing {}\n", rpt_file);
    parser.parse_file(rpt_file);
    _dbs.emplace_back(std::make_shared<basedb>(parser.get_db()));
  }
}

void flow_control::analyse() {
  comparator cmp(_configs, _dbs);
  cmp.match();
}

void flow_control::run() {
  parse_yml(_yml);
  for (const auto& [rpt, type] : _rpts) {
    parse_rpt(rpt, type);
  }
  analyse();
}
