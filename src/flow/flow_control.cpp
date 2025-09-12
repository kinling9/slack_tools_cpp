#include "flow/flow_control.h"

#include <absl/strings/match.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "absl/container/flat_hash_set.h"
#include "analyser/arc_analyser.h"
#include "analyser/comparator.h"
#include "analyser/existence_checker.h"
#include "analyser/fanout_analyser.h"
#include "analyser/pair_analyser.h"
#include "analyser/pair_analyser_csv.h"
#include "analyser/pair_analyser_dij.h"
#include "analyser/path_analyser.h"
#include "parser/csv_parser.h"
#include "parser/def_parser.h"
#include "parser/invs_rpt.h"
#include "parser/leda_endpoint.h"
#include "parser/leda_rpt.h"
#include "yaml-cpp/yaml.h"

void flow_control::parse_yml(std::string yml_file) {
  YAML::Node config;
  try {
    config = YAML::LoadFile(yml_file);
  } catch (const std::exception& err) {
    throw std::system_error(
        errno, std::generic_category(),
        fmt::format(fmt::fg(fmt::color::red), "cannot open yml file {}, {}",
                    yml_file, err.what()));
    std::exit(1);
  }
  fmt::print("Parsing {}\n", yml_file);
  std::cout << config << "\n";
  if (!config["mode"]) {
    throw std::system_error(
        errno, std::generic_category(),
        fmt::format(fmt::fg(fmt::color::red),
                    "Please provide the mode in the yml file."));
    std::exit(1);
  }
  if (!config["rpts"]) {
    throw std::system_error(
        errno, std::generic_category(),
        fmt::format(fmt::fg(fmt::color::red),
                    "Please provide the rpts in the yml file."));
    std::exit(1);
  }
  if (!config["configs"]) {
    throw std::system_error(
        errno, std::generic_category(),
        fmt::format(fmt::fg(fmt::color::red),
                    "Please provide the configs in the yml file."));
    std::exit(1);
  }
  auto mode = config["mode"].as<std::string>();
  _configs = configs{.mode = mode};
  if (mode == "compare") {
    _analyser = std::make_unique<comparator>(config["configs"]);
  } else if (mode == "cell in def") {
    _analyser = std::make_unique<existence_checker>(config["configs"]);
  } else if (mode == "arc analyse") {
    _analyser = std::make_unique<arc_analyser>(config["configs"]);
  } else if (mode == "fanout analyse") {
    _analyser = std::make_unique<fanout_analyser>(config["configs"]);
  } else if (mode == "path analyse") {
    _analyser = std::make_unique<path_analyser>(config["configs"]);
  } else if (mode == "pair analyse") {
    _analyser = std::make_unique<pair_analyser>(config["configs"]);
  } else if (mode == "pair analyse csv") {
    _analyser = std::make_unique<pair_analyser_csv>(config["configs"]);
  } else if (mode == "pair analyse dij") {
    _analyser = std::make_unique<pair_analyser_dij>(config["configs"]);
  } else {
    throw std::system_error(
        errno, std::generic_category(),
        fmt::format(fmt::fg(fmt::color::red),
                    "The mode {} is not supported, skip.", mode));
    std::exit(1);
  }
  auto config_node = config["configs"];
  if (!_analyser->parse_configs()) {
    throw std::system_error(errno, std::generic_category(),
                            fmt::format(fmt::fg(fmt::color::red),
                                        "Cannot parse the configs, skip."));
    std::exit(1);
  }
  auto rpt_node = config["rpts"];
  auto valid_rpts = _analyser->check_valid(rpt_node);

  std::vector<std::thread> threads;
  for (const auto& rpt : valid_rpts) {
    // threads.emplace_back([this, &rpt_node, rpt]() {
    //   run_function(fmt::format("parse rpt {}", rpt),
    //                [&]() { parse_rpt(rpt_node[rpt], rpt); });
    // });
    run_function(fmt::format("parse rpt {}", rpt),
                 [&]() { parse_rpt(rpt_node[rpt], rpt); });
  }
  // for (auto& t : threads) {
  //   if (t.joinable()) {
  //     t.join();
  //   }
  // }
}

void flow_control::parse_rpt(const YAML::Node& rpt, std::string key) {
  auto rpt_type = rpt["type"].as<std::string>();
  if (rpt_type == "csv") {
    auto parser = std::make_shared<csv_parser>();
    auto cell_csv_path = rpt["cell_csv"].as<std::string>();
    auto net_csv_path = rpt["net_csv"].as<std::string>();
    fmt::print("Parsing cell csv file {}\n", cell_csv_path);
    if (!parser->parse_file(csv_type::CellArc, cell_csv_path)) {
      throw std::system_error(
          errno, std::generic_category(),
          fmt::format(fmt::fg(fmt::color::red),
                      "Cannot parse cell csv file {}, skip.", cell_csv_path));
      std::exit(1);
    }

    auto parse_net_csv = [&](csv_type type) {
      fmt::print("Parsing net csv file {}\n", net_csv_path);
      if (!parser->parse_file(type, net_csv_path)) {
        throw std::system_error(
            errno, std::generic_category(),
            fmt::format(fmt::fg(fmt::color::red),
                        "Cannot parse net csv file {}, skip.", net_csv_path));
      }
    };

    if (rpt["at_csv"]) {
      parse_net_csv(csv_type::NetArcFanout);
      auto at_csv_path = rpt["at_csv"].as<std::string>();
      fmt::print("Parsing pin at csv file {}\n", at_csv_path);
      if (!parser->parse_file(csv_type::PinAT, at_csv_path)) {
        throw std::system_error(
            errno, std::generic_category(),
            fmt::format(fmt::fg(fmt::color::red),
                        "Cannot parse pin at csv file {}, skip.", at_csv_path));
      }
    } else {
      parse_net_csv(csv_type::NetArc);
    }
    {
      std::lock_guard<std::mutex> lock(_dbs_mutex);
      _dbs[key] = std::make_shared<basedb>(parser->get_db());
    }
    return;
  }
  auto rpt_file = rpt["path"].as<std::string>();
  bool ignore_path = false;
  if (rpt["ignore_path"]) {
    ignore_path = rpt["ignore_path"].as<bool>();
  }
  std::size_t max_paths = 0;
  if (rpt["max_paths"]) {
    max_paths = rpt["max_paths"].as<std::size_t>();
  }

  std::shared_ptr<basedb> cur_db;
  fmt::print("Parsing {}\n", rpt_file);
  std::variant<std::shared_ptr<rpt_parser<std::string>>,
               std::shared_ptr<rpt_parser<std::string_view>>>
      parser;
  absl::flat_hash_set<std::string> valid_types = {"leda", "leda_def", "invs",
                                                  "leda_endpoint"};
  if (valid_types.contains(rpt_type) == false) {
    throw std::system_error(errno, std::generic_category(),
                            fmt::format(fmt::fg(fmt::color::red),
                                        "Rpt type {} is not supported, "
                                        "skip.",
                                        rpt_type));
    std::exit(1);
  }

  if (rpt_type == "leda_endpoint") {
    parser = std::make_shared<leda_endpoint_parser<std::string_view>>(1);
  } else if (ignore_path) {
    if (absl::StrContains(rpt_type, "leda")) {
      parser = std::make_shared<leda_rpt_parser<std::string_view>>(1);
    } else if (absl::StrContains(rpt_type, "invs")) {
      parser = std::make_shared<invs_rpt_parser<std::string_view>>(1);
    }
    std::get<1>(parser)->set_ignore_blocks({Paths});
  } else {
    if (absl::StrContains(rpt_type, "leda")) {
      parser = std::make_shared<leda_rpt_parser<std::string>>();
    } else if (absl::StrContains(rpt_type, "invs")) {
      parser = std::make_shared<invs_rpt_parser<std::string>>();
    }
  }
  std::visit(
      [&](auto&& arg) {
        if (max_paths != 0) {
          arg->set_max_paths(max_paths);
        }
        if (arg->parse_file(rpt_file)) {
          // arg->print_paths();
          cur_db = std::make_shared<basedb>(arg->get_db());
          cur_db->type = rpt_type;
        } else {
          cur_db = nullptr;
        }
      },
      parser);
  // def as appendix
  if (absl::StrContains(rpt_type, "def")) {
    std::shared_ptr<def_parser> parser = std::make_shared<def_parser>();
    if (parser->parse_file(rpt["def"].as<std::string>())) {
      cur_db->update_loc_from_map(parser->get_loc_map());
    }
    cur_db->type_map = parser->get_type_map();
  }
  {
    std::lock_guard<std::mutex> lock(_dbs_mutex);
    _dbs[key] = cur_db;
  }
}

void flow_control::run() {
  parse_yml(_yml);
  _analyser->set_db(_dbs);
  _analyser->analyse();
}
