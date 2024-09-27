#pragma once
#include <re2/re2.h>

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>
#include <string>
#include <vector>

#include "parser/rpt_parser.h"
#include "utils/utils.h"

template <typename T>
class leda_rpt_parser : public rpt_parser<T> {
  using rpt_parser<T>::_ignore_blocks;

 public:
  leda_rpt_parser() : rpt_parser<T>("^Startpoint:", Beginpoint) {}
  leda_rpt_parser(int num_consumers)
      : rpt_parser<T>("^Startpoint:", num_consumers, Beginpoint) {}
  void update_iter(block &iter) override;
  void parse_line(T line, std::shared_ptr<data_block> &path_block) override;

 private:
  const RE2 _at_pattern{"^data arrival time"};
  const RE2 _begin_pattern{"^Startpoint: (\\S*)"};
  const RE2 _end_pattern{"^Endpoint: (\\S*)"};
  const RE2 _group_pattern{"^Path Group: (\\S*)$"};
  const RE2 _path_type_pattern{"^Path Type: (\\S*)$"};
  const RE2 _clock_pattern{"clocked\\s+by\\s+(.*?)\\)$"};
  const RE2 _slack_pattern{"^slack \\(\\S+\\)\\s+([0-9.-]*)"};
};

template <typename T>
void leda_rpt_parser<T>::update_iter(block &iter) {
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

template <typename T>
void leda_rpt_parser<T>::parse_line(T line,
                                    std::shared_ptr<data_block> &path_block) {
  if (_ignore_blocks.contains(path_block->iter)) {
    update_iter(path_block->iter);
    return;
  }
  switch (path_block->iter) {
    case Beginpoint:
      if (RE2::PartialMatch(line, _begin_pattern,
                            &path_block->path_obj->startpoint)) {
        RE2::PartialMatch(line, _clock_pattern, &path_block->path_obj->clock);
        update_iter(path_block->iter);
      }
      break;
    case Endpoint:
      if (RE2::PartialMatch(line, _end_pattern,
                            &path_block->path_obj->endpoint)) {
        update_iter(path_block->iter);
      }
      break;
    case PathGroup:
      if (RE2::FullMatch(line, _group_pattern, &path_block->path_obj->group)) {
        update_iter(path_block->iter);
      }
      break;
    case PathType:
      if (RE2::FullMatch(line, _path_type_pattern)) {
        update_iter(path_block->iter);
      }
      break;
    case Paths: {
      if (RE2::PartialMatch(line, _at_pattern)) {
        update_iter(path_block->iter);
        break;
      }
      // Parse the path
      // TODO: auto generate token iter from title line
      std::vector<std::string_view> tokens = split_string_by_spaces(line);
      if (tokens.size() == 8) {
        Pin pin;
        pin.is_input = path_block->is_input;
        path_block->is_input = !path_block->is_input;
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
        path_block->pin_obj = std::make_shared<Pin>(pin);
        if (path_block->net_obj->pins.second == nullptr) {
          path_block->net_obj->pins.second = path_block->pin_obj;
          path_block->pin_obj->net = path_block->net_obj;
          path_block->path_obj->path.push_back(path_block->pin_obj);
        }
      } else if (tokens.size() == 3) {
        Net net;
        net.name = std::string(tokens[0]);
        net.fanout =
            boost::convert<int>(tokens[1], boost::cnv::strtol()).value();
        net.cap =
            boost::convert<double>(tokens[2], boost::cnv::strtol()).value();
        net.pins = std::make_pair(path_block->pin_obj, nullptr);
        path_block->net_obj = std::make_shared<Net>(net);
        path_block->pin_obj->net = path_block->net_obj;
        path_block->path_obj->path.push_back(path_block->pin_obj);
      }
      break;
    }
    case Slack: {
      T path_slack;
      if (RE2::PartialMatch(line, _slack_pattern, &path_slack)) {
        if (!path_slack.empty()) {
          path_block->path_obj->slack =
              boost::convert<double>(path_slack, boost::cnv::strtol()).value();
        }
        update_iter(path_block->iter);
      }
      break;
    }
    default:
      break;
  }
}
