#include "parser/leda_rpt.h"

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>

#include "parser/rpt_parser.h"
#include "utils/utils.h"

void leda_rpt_parser::update_iter(block &iter) {
  switch (iter) {
    case Beginpoint:
      iter = Endpoint;
      break;
    case Endpoint:
      iter = PathGroup;
      break;
    case PathGroup:
      iter = PathType;
      break;
    case PathType:
      iter = Paths;
      break;
    case Paths:
      iter = Slack;
      break;
    case Slack:
      iter = End;
      break;
    default:
      break;
  }
}

std::shared_ptr<Path> leda_rpt_parser::parse_path(
    const std::vector<std::string> &path) {
  std::shared_ptr<Path> pathObj = std::make_shared<Path>();
  std::shared_ptr<Pin> pinObj = std::make_shared<Pin>();
  std::shared_ptr<Net> netObj = std::make_shared<Net>();
  block iter = Beginpoint;
  std::string path_slack;
  for (const auto &line : path) {
    if (_ignore.contains(iter)) {
      update_iter(iter);
      continue;
    }
    switch (iter) {
      case Beginpoint:
        if (RE2::FullMatch(line, _begin_pattern, &pathObj->startpoint)) {
          RE2::PartialMatch(line, _clock_pattern, &pathObj->clock);
          update_iter(iter);
        }
        break;
      case Endpoint:
        if (RE2::FullMatch(line, _end_pattern, &pathObj->endpoint)) {
          update_iter(iter);
        }
        break;
      case PathGroup:
        if (RE2::FullMatch(line, _group_pattern, &pathObj->group)) {
          update_iter(iter);
        }
        break;
      case PathType:
        if (RE2::FullMatch(line, _path_type_pattern)) {
          update_iter(iter);
        }
        break;
      case Paths: {
        if (RE2::FullMatch(line, _at_pattern)) {
          update_iter(iter);
          break;
        }
        // Parse the path
        std::vector<std::string_view> tokens = split_string_by_spaces(line);
        if (tokens.size() == 8) {
          Pin pin;
          pin.name = std::string(tokens[0]);
          pin.cell = std::string(tokens[1].substr(1, tokens[1].size() - 2));
          pin.trans =
              boost::convert<double>(tokens[2], boost::cnv::strtol()).value();
          pin.incr_delay =
              boost::convert<double>(tokens[3], boost::cnv::strtol()).value();
          pin.path_delay =
              boost::convert<double>(tokens[4], boost::cnv::strtol()).value();
          pin.rise_fall = tokens[5] == "r";
          pin.location = std::make_pair(
              boost::convert<double>(tokens[6].substr(1, tokens[6].size() - 2),
                                     boost::cnv::strtol())
                  .value(),
              boost::convert<double>(tokens[7].substr(0, tokens[7].size() - 2),
                                     boost::cnv::strtol())
                  .value());
          pinObj = std::make_shared<Pin>(pin);
          if (netObj->pins.second == nullptr) {
            netObj->pins.second = pinObj;
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
          }
        } else if (tokens.size() == 3) {
          Net net;
          net.name = std::string(tokens[0]);
          net.fanout =
              boost::convert<int>(tokens[1], boost::cnv::strtol()).value();
          net.cap =
              boost::convert<double>(tokens[2], boost::cnv::strtol()).value();
          net.pins = std::make_pair(pinObj, nullptr);
          netObj = std::make_shared<Net>(net);
          pinObj->net = netObj;
          pathObj->path.push_back(pinObj);
        }
        break;
      }
      case Slack:
        if (RE2::FullMatch(line, _slack_pattern, &path_slack)) {
          update_iter(iter);
        }
        break;
      default:
        break;
    }
  }
  if (!path_slack.empty()) {
    pathObj->slack =
        boost::convert<double>(path_slack, boost::cnv::strtol()).value();
  }
  return pathObj;
}
