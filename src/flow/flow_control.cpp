#include "flow/flow_control.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include "absl/container/flat_hash_set.h"
#include "analyser/arc_analyser.h"
#include "analyser/comparator.h"
#include "analyser/existence_checker.h"
#include "parser/def_parser.h"
#include "parser/invs_rpt.h"
#include "parser/leda_endpoint.h"
#include "parser/leda_rpt.h"
#include "utils/design_cons.h"
#include "utils/double_filter/double_filter.h"
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
  if (!config["mode"]) {
    throw fmt::system_error(errno, "Please provide the mode in the yml file.");
    std::exit(1);
  }
  if (!config["rpts"]) {
    throw fmt::system_error(errno, "Please provide the rpts in the yml file.");
    std::exit(1);
  }
  if (!config["configs"]) {
    throw fmt::system_error(errno,
                            "Please provide the configs in the yml file.");
    std::exit(1);
  }
  auto mode = config["mode"].as<std::string>();
  _configs = configs{.mode = mode};
  if (mode == "compare") {
    _analyser = std::make_unique<comparator>(config["configs"]);
    // parse_rpt_config(config);
    // _configs.compare_mode = config["compare_mode"].as<std::string>();
    // if (config["slack_margins"]) {
    //   _configs.slack_margins =
    //       config["slack_margins"].as<std::vector<double>>();
    // }
    // if (config["match_percentages"]) {
    //   _configs.match_percentages =
    //       config["match_percentages"].as<std::vector<double>>();
    // }
    // if (config["match_paths"]) {
    //   _configs.match_paths = config["match_paths"].as<std::size_t>();
    // }
    // if (config["enable_mbff"]) {
    //   _configs.enable_mbff = config["enable_mbff"].as<bool>();
    // }
    // if (config["slack_filter"]) {
    //   compile_double_filter(config["slack_filter"].as<std::string>(),
    //                         _configs.slack_filter_op_code);
    // }
    // if (config["diff_filter"]) {
    //   compile_double_filter(config["diff_filter"].as<std::string>(),
    //                         _configs.diff_filter_op_code);
    // }
  } else if (mode == "cell in def") {
    _analyser = std::make_unique<existence_checker>(config["configs"]);
    // _rpt_defs = config["rpt_defs"].as<std::vector<std::vector<std::string>>>();
    // _rpt_tool = config["tool"].as<std::string>();
  } else if (mode == "arc analyse") {
    _analyser = std::make_unique<arc_analyser>(config["configs"]);
    // parse_rpt_config(config);
    // if (config["delay_filter"]) {
    //   compile_double_filter(config["delay_filter"].as<std::string>(),
    //                         _configs.delay_filter_op_code);
    // }
    // if (config["fanout_filter"]) {
    //   compile_double_filter(config["fanout_filter"].as<std::string>(),
    //                         _configs.fanout_filter_op_code);
    // }
  } else {
    throw fmt::system_error(errno, "The mode {} is not supported, skip.", mode);
    std::exit(1);
  }
  auto config_node = config["configs"];
  if (!_analyser->parse_configs()) {
    throw fmt::system_error(errno, "Cannot parse the configs, skip.");
    std::exit(1);
  }
  auto rpt_node = config["rpts"];
  auto valid_rpts = _analyser->check_valid(rpt_node);
  for (const auto& rpt : valid_rpts) {
    run_function(fmt::format("parse rpt {}", rpt),
                 [&]() { parse_rpt(rpt_node[rpt], rpt); });
  }
}

// void flow_control::parse_rpt_config(YAML::Node& config) {
//   design_cons& cons = design_cons::get_instance();
//   auto tools = config["tools"].as<std::vector<std::string>>();
//   auto rpts = config["rpts"].as<std::vector<std::vector<std::string>>>();
//   if (tools.size() != 2) {
//     throw fmt::system_error(errno, "Please provide two files for comparison");
//     std::exit(1);
//   }
//   for (std::size_t i = 0; i < rpts.size(); i++) {
//     const auto& rpt_group = rpts[i];
//     if (rpt_group.size() != 2) {
//       fmt::print("Rpts {} should contain two files, ignore this pair.\n", i);
//     } else {
//       std::string design;
//       absl::flat_hash_set<std::string> designs;
//       bool design_found = true;
//       for (const auto& rpt : rpt_group) {
//         auto design_name = cons.get_name(rpt);
//         if (design_name == "") {
//           fmt::print(
//               "The design period corresponding to rpt {} cannot be found, "
//               "skip "
//               "rpt pair {}.\n",
//               rpt, i);
//           design_found = false;
//         }
//         designs.emplace(design_name);
//       }
//       if (!design_found) {
//         continue;
//       }
//       if (designs.size() != 1) {
//         fmt::print(
//             "The reports are generated from different designs {}, skip rpt "
//             "pair {}.\n",
//             fmt::join(designs, ","), i);
//         continue;
//       }
//       design = designs.begin()->data();
//       _rpts.insert(
//           {design, {{rpt_group[0], tools[0]}, {rpt_group[1], tools[1]}}});
//     }
//   }
// }

void flow_control::parse_rpt(const YAML::Node& rpt, std::string key) {
  auto rpt_file = rpt["path"].as<std::string>();
  auto rpt_type = rpt["type"].as<std::string>();
  bool ignore_path = false;
  if (rpt["ignore_path"]) {
    ignore_path = rpt["ignore_path"].as<bool>();
  }

  std::shared_ptr<basedb> cur_db;
  fmt::print("Parsing {}\n", rpt_file);
  std::variant<std::shared_ptr<rpt_parser<std::string>>,
               std::shared_ptr<rpt_parser<std::string_view>>>
      parser;
  absl::flat_hash_set<std::string> valid_types = {"leda", "invs",
                                                  "leda_endpoint"};
  if (valid_types.contains(rpt_type) == false) {
    throw fmt::system_error(errno, "The type {} is not supported, skip.",
                            rpt_type);
    std::exit(1);
  }
  // if (rpt_type == "leda_endpoint" && _configs.compare_mode != "endpoint") {
  //   throw fmt::system_error(
  //       errno,
  //       "The type {} is only supported in compare mode with "
  //       "endpoints, skip.",
  //       rpt_type);
  //   std::exit(1);
  // }

  if (rpt_type == "leda_endpoint") {
    parser = std::make_shared<leda_endpoint_parser<std::string_view>>(1);
  } else if (ignore_path) {
    if (rpt_type == "leda") {
      parser = std::make_shared<leda_rpt_parser<std::string_view>>(1);
    } else if (rpt_type == "invs") {
      parser = std::make_shared<invs_rpt_parser<std::string_view>>(1);
    }
    std::get<1>(parser)->set_ignore_blocks({Paths});
  } else {
    if (rpt_type == "leda") {
      parser = std::make_shared<leda_rpt_parser<std::string>>();
    } else if (rpt_type == "invs") {
      parser = std::make_shared<invs_rpt_parser<std::string>>();
    }
  }
  std::visit(
      [&](auto&& arg) {
        if (arg->parse_file(rpt_file)) {
          cur_db = std::make_shared<basedb>(arg->get_db());
          cur_db->type = rpt_type;
        } else {
          cur_db = nullptr;
        }
      },
      parser);
  _dbs[key] = cur_db;
}

void flow_control::run() {
  // TODO: rewrite to handle the data sharing between classes
  parse_yml(_yml);
  _analyser->set_db(_dbs);
  _analyser->analyse();
}
