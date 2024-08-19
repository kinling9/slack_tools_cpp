#include <fmt/ranges.h>

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>

#include "dm/dm.h"
#include "parser/invs_rpt.h"
#include "utils/utils.h"

enum block {
  Endpoint = 0,
  Beginpoint = 1,
  PathGroup = 2,
  Slack = 3,
  Paths = 4,
};

std::shared_ptr<Path> invs_rpt_parser::parse_path(
    const std::vector<std::string> &path) {
  std::shared_ptr<Path> pathObj = std::make_shared<Path>();
  std::shared_ptr<Pin> pinObj = std::make_shared<Pin>();
  std::shared_ptr<Net> netObj = std::make_shared<Net>();
  int iter = 0;
  int split_count = 0;
  std::string path_slack;

  for (const auto &line : path) {
    switch (iter) {
      case Endpoint:
        if (RE2::FullMatch(line, _end_pattern, &pathObj->endpoint)) {
          iter++;
        }
        break;
      case Beginpoint:
        if (RE2::FullMatch(line, _begin_pattern, &pathObj->startpoint)) {
          RE2::PartialMatch(line, _clock_pattern, &pathObj->clock);
          iter++;
        }
        break;
      case PathGroup:
        if (RE2::FullMatch(line, _group_pattern, &pathObj->group)) {
          iter++;
        }
        break;
      case Slack:
        if (RE2::FullMatch(line, _slack_pattern, &path_slack)) {
          pathObj->slack =
              boost::convert<double>(path_slack, boost::cnv::strtol()).value();
          iter++;
        }
        break;
      case Paths:
        if (RE2::FullMatch(line, _split_pattern)) {
          ++split_count;
        }
        break;
    }
  }

  std::cout << "Startpoint: " << pathObj->startpoint
            << " Endpoint: " << pathObj->endpoint
            << " Group: " << pathObj->group << " Clock: " << pathObj->clock
            << " Slack: " << pathObj->slack << "\n";
  return pathObj;
}
