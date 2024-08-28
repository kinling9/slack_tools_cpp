#include <fmt/core.h>
#include <fmt/ranges.h>

#include "absl/container/flat_hash_set.h"
#include "analyser/comparator.h"
#include "analyser/existence_checker.h"
#include "flow/flow_control.h"
#include "parser/def_parser.h"
#include "parser/invs_rpt.h"
#include "parser/leda_rpt.h"
#include "utils/design_cons.h"
#include "yaml-cpp/yaml.h"

void flow_control::parse_yml(std::string yml_file) {
  YAML::Node config;
  design_cons& cons = design_cons::get_instance();
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
  _configs = configs{.mode = mode};
  if (config["output_dir"]) {
    _configs.output_dir = config["output_dir"].as<std::string>();
  }
  if (mode == "compare") {
    auto types = config["type"].as<std::vector<std::string>>();
    auto rpts = config["rpts"].as<std::vector<std::vector<std::string>>>();
    if (types.size() != 2) {
      throw fmt::system_error(errno, "Please provide two files for comparison");
      std::exit(1);
    }
    for (std::size_t i = 0; i < rpts.size(); i++) {
      const auto& rpt_group = rpts[i];
      if (rpt_group.size() != 2) {
        fmt::print("Rpts {} should contain two files, ignore this pair.\n", i);
      } else {
        std::string design;
        absl::flat_hash_set<std::string> designs;
        bool design_found = true;
        for (const auto& rpt : rpt_group) {
          auto design_name = cons.get_name(rpt);
          if (design_name == "") {
            fmt::print(
                "The design period corresponding to rpt {} cannot be found, "
                "skip "
                "rpt pair {}.\n",
                rpt, i);
            design_found = false;
          }
          designs.emplace(design_name);
        }
        if (!design_found) {
          continue;
        }
        if (designs.size() != 1) {
          fmt::print(
              "The reports are generated from different designs {}, skip rpt "
              "pair {}.\n",
              fmt::join(designs, ","), i);
          continue;
        }
        design = designs.begin()->data();
        _rpts.insert(
            {design, {{rpt_group[0], types[0]}, {rpt_group[1], types[1]}}});
      }
    }
    _configs.compare_mode = config["compare_mode"].as<std::string>();
    for (std::size_t i = 0; i < _rpts.size(); i++) {
      std::string design;
      absl::flat_hash_set<std::string> designs;
      auto rpt_group = rpts[i];
      for (const auto& rpt : rpt_group) {
        auto design_name = cons.get_name(rpt);
        if (design_name == "") {
          fmt::print(
              "The design period corresponding to rpt {} cannot be found, skip "
              "rpt pair {}.\n",
              rpt, i);
        }
        designs.emplace(design_name);
      }
      if (designs.size() != 1) {
        fmt::print(
            "The reports are generated from different designs {}, skip rpt "
            "pair {}.\n",
            fmt::join(designs, ","), i);
      }
      design = designs.begin()->data();
      // _configs.design_names.push_back(design);
    }
    if (config["slack_margins"]) {
      _configs.slack_margins =
          config["slack_margins"].as<std::vector<double>>();
    }
    if (config["match_percentages"]) {
      _configs.match_percentages =
          config["match_percentages"].as<std::vector<double>>();
    }
  } else if (mode == "cell in def") {
    _rpt_defs = config["rpt_defs"].as<std::vector<std::vector<std::string>>>();
    _rpt_type = config["rpt_type"].as<std::string>();
  } else {
    throw fmt::system_error(errno, "The mode {} is not supported, skip.", mode);
    std::exit(1);
  }
}

std::shared_ptr<basedb> flow_control::parse_rpt(std::string rpt_file,
                                                std::string rpt_type) {
  std::shared_ptr<basedb> cur_db;
  fmt::print("Parsing {}\n", rpt_file);
  if (rpt_type == "leda") {
    leda_rpt_parser parser;
    if (parser.parse_file(rpt_file)) {
      cur_db = std::make_shared<basedb>(parser.get_db());
    } else {
      cur_db = nullptr;
    }
  } else if (rpt_type == "invs") {
    invs_rpt_parser parser(1);
    if (parser.parse_file(rpt_file)) {
      cur_db = std::make_shared<basedb>(parser.get_db());
    } else {
      cur_db = nullptr;
    }
  } else {
    throw fmt::system_error(errno, "The report type {} is not supported, skip.",
                            rpt_type);
    std::exit(1);
    cur_db = nullptr;
  }
  return cur_db;
}

void flow_control::analyse() {
  if (_configs.mode == "compare") {
    comparator cmp(_configs, _dbs);
    cmp.analyse();
  } else if (_configs.mode == "cell in def") {
    existence_checker checker(_configs);
    for (const auto& rpt_def : _rpt_defs) {
      const auto& rpt = rpt_def[0];
      const auto& def = rpt_def[1];
      const auto& db = parse_rpt(rpt, _rpt_type);
      def_parser parser;
      parser.parse_file(def);
      checker.check_existence(parser.get_map(), db);
    }
  } else {
    throw fmt::system_error(errno, "The mode {} is not supported, skip.",
                            _configs.mode);
    std::exit(1);
  }
}

void flow_control::parse() {
  if (_configs.mode == "compare") {
    for (const auto& [design, rpt_group] : _rpts) {
      std::vector<std::shared_ptr<basedb>> db_group;
      for (const auto& [rpt, type] : rpt_group) {
        const auto& db = parse_rpt(rpt, type);
        if (db != nullptr) {
          db_group.push_back(db);
        }
      }
      if (db_group.size() != 2) {
        fmt::print(
            "The number of valid reports for design {} is not 2, skip.\n",
            design);
        continue;
      }
      _dbs.insert({design, db_group});
    }
  }
}

void flow_control::run() {
  // TODO: rewrite to handle the data sharing between classes
  parse_yml(_yml);
  parse();
  analyse();
}
