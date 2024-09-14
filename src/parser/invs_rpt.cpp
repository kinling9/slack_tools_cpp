#include "parser/invs_rpt.h"

#include <absl/container/flat_hash_map.h>
#include <fmt/ranges.h>

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>

#include "dm/dm.h"
#include "utils/utils.h"

void invs_rpt_parser::update_iter(block &iter) {
  switch (iter) {
    case Endpoint:
      iter = Beginpoint;
      break;
    case Beginpoint:
      iter = PathGroup;
      break;
    case PathGroup:
      iter = Slack;
      break;
    case Slack:
      iter = Paths;
      break;
    case Paths:
      iter = End;
      break;
    default:
      break;
  }
}

std::shared_ptr<Path> invs_rpt_parser::parse_path(
    const std::vector<std::string> &path) {
  std::shared_ptr<Path> pathObj = std::make_shared<Path>();
  std::shared_ptr<Pin> pinObj = std::make_shared<Pin>();
  std::shared_ptr<Net> netObj = std::make_shared<Net>();
  block iter = Endpoint;
  int split_count = 0;
  bool with_net = false;
  std::string path_slack;
  absl::flat_hash_map<std::string, std::size_t> row;
  std::string headers;

  for (const auto &line : path) {
    if (_ignore.contains(iter)) {
      update_iter(iter);
      continue;
    }
    switch (iter) {
      case Endpoint:
        if (RE2::PartialMatch(line, _end_pattern, &pathObj->endpoint)) {
          update_iter(iter);
        }
        break;
      case Beginpoint:
        if (RE2::PartialMatch(line, _begin_pattern, &pathObj->startpoint)) {
          RE2::PartialMatch(line, _clock_pattern, &pathObj->clock);
          update_iter(iter);
        }
        break;
      case PathGroup:
        if (RE2::FullMatch(line, _group_pattern, &pathObj->group)) {
          update_iter(iter);
        }
        break;
      case Slack:
        if (RE2::PartialMatch(line, _slack_pattern, &path_slack)) {
          pathObj->slack =
              boost::convert<double>(path_slack, boost::cnv::strtol())
                  .value_or(0);
          update_iter(iter);
        }
        break;
      case Paths:
        if (RE2::PartialMatch(line, _split_pattern)) {
          ++split_count;
          continue;
        }
        if (split_count == 1) {
          if (headers.empty()) {
            headers = line;
          } else {
            auto tokens0 = split_string_by_n_spaces(headers, 2);
            auto tokens1 = split_string_by_n_spaces(line, 2);
            absl::flat_hash_map<std::size_t, std::string_view> map_line1;
            for (const auto &[start, str] : tokens1) {
              map_line1[start] = str;
            }
            std::size_t i = 0;
            for (const auto &[start, str] : tokens0) {
              std::string key;
              if (!map_line1.contains(start)) {
                key = std::string(str);
              } else {
                key = fmt::format("{} {}", str, map_line1[start]);
              }
              row[key] = i++;
            }
          }
        } else if (split_count == 2) {
          auto splits = split_string_by_n_spaces(line, 2);
          std::vector<std::string_view> tokens;
          std::ranges::transform(splits, std::back_inserter(tokens),
                                 [](const auto &pair) { return pair.second; });
          Pin pin;
          pin.name = std::string(tokens[row["Pin"]]);
          pin.trans =
              boost::convert<double>(tokens[row["Slew"]], boost::cnv::strtol())
                  .value_or(0);
          pin.path_delay = boost::convert<double>(tokens[row["Arrival Time"]],
                                                  boost::cnv::strtol())
                               .value_or(0);
          if (tokens[row["Instance Location"]] == "-") {
            pinObj = std::make_shared<Pin>(pin);
            Net net;
            if (row.contains("Net")) {
              net.name = std::string(tokens[row["Net"]]);
            }
            net.pins = std::make_pair(pinObj, nullptr);
            netObj = std::make_shared<Net>(net);
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
            continue;
          }
          pin.rise_fall = tokens[row["Edge"]] == "^";
          pin.cell = std::string(tokens[row["Cell"]]);
          pin.incr_delay = boost::convert<double>(tokens[row["Incr Delay"]],
                                                  boost::cnv::strtol())
                               .value_or(0);
          auto space_index = tokens[row["Instance Location"]].find(' ');
          if (space_index != std::string::npos) {
            pin.location = std::make_pair(
                boost::convert<double>(
                    tokens[row["Instance Location"]].substr(1, space_index - 2),
                    boost::cnv::strtol())
                    .value_or(0),
                boost::convert<double>(
                    tokens[row["Instance Location"]].substr(
                        space_index + 1,
                        tokens[row["Instance Location"]].size() - space_index -
                            2),
                    boost::cnv::strtol())
                    .value_or(0));
          }
          pinObj = std::make_shared<Pin>(pin);
          if (netObj->pins.second == nullptr) {
            netObj->pins.second = pinObj;
            netObj->fanout =
                boost::convert<int>(tokens[row["Fanout"]], boost::cnv::strtol())
                    .value_or(0);
            netObj->cap = boost::convert<double>(tokens[row["Load"]],
                                                 boost::cnv::strtol())
                              .value_or(0);
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
          } else {
            Net net;
            if (with_net) {
              net.name = std::string(tokens[row["Net"]]);
            }
            net.pins = std::make_pair(pinObj, nullptr);
            netObj = std::make_shared<Net>(net);
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
          }
        } else if (split_count == 3) {
          update_iter(iter);
        }
        break;
      default:
        break;
    }
  }
  return pathObj;
}
