#include "flow/flow_control.h"

#include <fmt/color.h>
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
  for (const auto& rpt : valid_rpts) {
    run_function(fmt::format("parse rpt {}", rpt),
                 [&]() { parse_rpt(rpt_node[rpt], rpt); });
  }
}

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
