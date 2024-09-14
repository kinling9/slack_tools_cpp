#pragma once
#include <re2/re2.h>

#include <string>
#include <vector>

#include "parser/rpt_parser.h"

class invs_rpt_parser : public rpt_parser {
 public:
  invs_rpt_parser() : rpt_parser("invs", "^Path \\d") {}
  invs_rpt_parser(int num_consumers)
      : rpt_parser("invs", "^Path \\d", num_consumers) {}
  std::shared_ptr<Path> parse_path(
      const std::vector<std::string> &path) override;
  void update_iter(block &iter) override;

 private:
  const RE2 _split_pattern{"^      --"};
  const RE2 _begin_pattern{"^Beginpoint: (\\S*)"};
  const RE2 _end_pattern{"^Endpoint:   (\\S*)"};
  const RE2 _group_pattern{"^Path Groups: {(\\S*)}"};
  const RE2 _clock_pattern{"triggered by\\s+leading edge of '(\\S*)'$"};
  const RE2 _slack_pattern{"^= Slack Time\\s+([0-9\\.-]*)"};
};
