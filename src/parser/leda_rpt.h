#pragma once
#include <re2/re2.h>

#include <iostream>
#include <string>
#include <vector>

#include "parser/parser.h"

class leda_rpt_parser : public parser {
 public:
  void parse_path(const std::vector<std::string> &path);

 private:
  const RE2 at_pattern_{"^data arrival time.*"};
  const RE2 begin_pattern_{"^Startpoint: (\\S*) .*"};
  const RE2 end_pattern_{"^Endpoint: (\\S*) .*"};
  const RE2 group_pattern_{"^Path Group: (\\S*)"};
  const RE2 path_type_pattern_{"^Path Type: (\\S*)"};
  const RE2 clock_pattern_{"clocked\\s+by\\s+(.*?)\\)"};
};
