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
  bool with_net = false;
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
          continue;
        }
        if (split_count == 1) {
          if (RE2::FullMatch(line, _with_net_pattern)) {
            with_net = true;
            continue;
          }
        } else if (split_count == 2) {
          std::vector<std::string_view> tokens = split_string_by_spaces(line);
          Pin pin;
          pin.name = std::string(tokens[0]);
          pin.trans = boost::convert<double>(tokens[tokens.size() - 8],
                                             boost::cnv::strtol())
                          .value();
          pin.path_delay = boost::convert<double>(tokens[tokens.size() - 1],
                                                  boost::cnv::strtol())
                               .value();
          if ((tokens.size() == 13 && with_net) || tokens.size() == 12) {
            pinObj = std::make_shared<Pin>(pin);
            Net net;
            if (with_net) {
              net.name = std::string(tokens[3]);
            }
            net.pins = std::make_pair(pinObj, nullptr);
            netObj = std::make_shared<Net>(net);
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
            continue;
          }
          pin.rise_fall = tokens[1] == "^";
          pin.cell = std::string(tokens[2]);
          pin.incr_delay = boost::convert<double>(tokens[tokens.size() - 10],
                                                  boost::cnv::strtol())
                               .value();
          pin.location =
              std::make_pair(boost::convert<double>(
                                 tokens[tokens.size() - 5].substr(
                                     1, tokens[tokens.size() - 5].size() - 2),
                                 boost::cnv::strtol())
                                 .value(),
                             boost::convert<double>(
                                 tokens[tokens.size() - 4].substr(
                                     0, tokens[tokens.size() - 4].size() - 2),
                                 boost::cnv::strtol())
                                 .value());
          pinObj = std::make_shared<Pin>(pin);
          if (netObj->pins.second == nullptr) {
            netObj->pins.second = pinObj;
            netObj->fanout = boost::convert<int>(tokens[tokens.size() - 3],
                                                 boost::cnv::strtol())
                                 .value_or(0);
            netObj->cap = boost::convert<double>(tokens[tokens.size() - 8],
                                                 boost::cnv::strtol())
                              .value_or(0);
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
          } else {
            Net net;
            if (with_net) {
              net.name = std::string(tokens[3]);
            }
            net.pins = std::make_pair(pinObj, nullptr);
            netObj = std::make_shared<Net>(net);
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
          }
        } else if (split_count == 3) {
          iter++;
        }
        break;
    }
  }
  return pathObj;
}
