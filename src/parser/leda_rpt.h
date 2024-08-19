#pragma once
#include <re2/re2.h>

#include <string>
#include <vector>

#include "parser/parser.h"

class leda_rpt_parser : public parser {
 public:
  leda_rpt_parser() : parser("Startpoint: .*") {}
  leda_rpt_parser(int num_consumers)
      : parser("Startpoint: .*", num_consumers) {}
  std::shared_ptr<Path> parse_path(const std::vector<std::string> &path);

 private:
  const RE2 _at_pattern{"^data arrival time.*"};
  const RE2 _begin_pattern{"^Startpoint: (\\S*) .*"};
  const RE2 _end_pattern{"^Endpoint: (\\S*) .*"};
  const RE2 _group_pattern{"^Path Group: (\\S*)"};
  const RE2 _path_type_pattern{"^Path Type: (\\S*)"};
  const RE2 _clock_pattern{"clocked\\s+by\\s+(.*?)\\)"};
  const RE2 _slack_pattern{"^slack \\(\\S+\\)\\s+([0-9.-]*).*"};
};
