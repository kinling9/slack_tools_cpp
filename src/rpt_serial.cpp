#include <fmt/ranges.h>

#include <argparse/argparse.hpp>
#include <iostream>

#include "parser/leda_rpt.h"

int main(int argc, char** argv) {
  argparse::ArgumentParser program("rpt_serial");
  program.add_argument("rpt_path").help("leda rpt file path");
  program.add_argument("output_path").help("leda rpt json file output path");

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  auto parser = std::make_shared<leda_rpt_parser<std::string>>();
  parser->parse_file(program.get<std::string>("rpt_path"));
  parser->get_db().serialize_to_yyjson(program.get<std::string>("output_path"));
  return 0;
}
