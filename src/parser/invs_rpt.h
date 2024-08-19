#pragma once
#include <re2/re2.h>

#include <string>
#include <vector>

#include "parser/parser.h"

class invs_rpt_parser : public parser {
 public:
  invs_rpt_parser() : parser("^Path\\s+\\d+.*") {}
  invs_rpt_parser(int num_consumers)
      : parser("Path\\s+\\d+.*", num_consumers) {}
  std::shared_ptr<Path> parse_path(const std::vector<std::string> &path);

 private:
  const RE2 _split_pattern{"^\\s+-+.*"};
  const RE2 _begin_pattern{"^Beginpoint:\\s+(\\S*)\\s+.*"};
  const RE2 _end_pattern{"^Endpoint:\\s+(\\S*)\\s+.*"};
  const RE2 _group_pattern{"^Path Groups: {(\\S*)}"};
  const RE2 _clock_pattern{"triggered by\\s+leading edge of\\s+'(.*)'"};
  const RE2 _slack_pattern{"^= Slack Time\\s+([0-9\\.-]*).*"};
};
