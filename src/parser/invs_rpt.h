#pragma once
#include <absl/container/flat_hash_map.h>
#include <fmt/ranges.h>
#include <re2/re2.h>

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>
#include <string>
#include <vector>

#include "dm/dm.h"
#include "parser/rpt_parser.h"
#include "utils/utils.h"

template <typename T>
class invs_rpt_parser : public rpt_parser<T> {
  using rpt_parser<T>::_ignore_blocks;
  using rpt_parser<T>::set_ignore_blocks;

 public:
  invs_rpt_parser() : rpt_parser<T>("^Path \\d", Endpoint) {}
  invs_rpt_parser(int num_consumers)
      : rpt_parser<T>("^Path \\d", num_consumers, Endpoint) {}
  void update_iter(block &iter) override;
  void parse_line(T line, std::shared_ptr<data_block> &path_block) override;

 private:
  const RE2 _split_pattern{"^      --"};
  const RE2 _begin_pattern{"^Beginpoint: (\\S*)"};
  const RE2 _end_pattern{"^Endpoint:   (\\S*)"};
  const RE2 _group_pattern{"^Path Groups: {(\\S*)}"};
  const RE2 _clock_pattern{"triggered by\\s+leading edge of '(\\S*)'$"};
  const RE2 _slack_pattern{"^= Slack Time\\s+([0-9\\.-]*)"};
};

template <typename T>
void invs_rpt_parser<T>::update_iter(block &iter) {
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

template <typename T>
void invs_rpt_parser<T>::parse_line(T line,
                                    std::shared_ptr<data_block> &path_block) {
  if (_ignore_blocks.contains(path_block->iter)) {
    update_iter(path_block->iter);
    return;
  }
  switch (path_block->iter) {
    case Endpoint:
      if (RE2::PartialMatch(line, _end_pattern,
                            &path_block->path_obj->endpoint)) {
        update_iter(path_block->iter);
      }
      break;
    case Beginpoint:
      if (RE2::PartialMatch(line, _begin_pattern,
                            &path_block->path_obj->startpoint)) {
        RE2::PartialMatch(line, _clock_pattern, &path_block->path_obj->clock);
        update_iter(path_block->iter);
      }
      break;
    case PathGroup:
      // fix condition when dual group
      if (RE2::PartialMatch(line, _group_pattern,
                            &path_block->path_obj->group)) {
        update_iter(path_block->iter);
      }
      break;
    case Slack: {
      T path_slack;
      if (RE2::PartialMatch(line, _slack_pattern, &path_slack)) {
        path_block->path_obj->slack =
            boost::convert<double>(path_slack, boost::cnv::strtol())
                .value_or(0);
        update_iter(path_block->iter);
      }
      break;
    }
    case Paths:
      if (RE2::PartialMatch(line, _split_pattern)) {
        ++path_block->split_count;
        return;
      }
      if (path_block->split_count == 1) {
        if (path_block->headers.empty()) {
          path_block->headers = line;
        } else {
          auto tokens0 = split_string_by_n_spaces(path_block->headers, 2);
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
            path_block->row[key] = i++;
          }
        }
      } else if (path_block->split_count == 2) {
        auto splits = split_string_by_n_spaces(line, 2);
        std::vector<std::string_view> tokens;
        std::ranges::transform(splits, std::back_inserter(tokens),
                               [](const auto &pair) { return pair.second; });
        Pin pin;
        pin.name = std::string(tokens[path_block->row["Pin"]]);
        pin.trans = boost::convert<double>(tokens[path_block->row["Slew"]],
                                           boost::cnv::strtol())
                        .value_or(0);
        pin.path_delay =
            boost::convert<double>(tokens[path_block->row["Arrival Time"]],
                                   boost::cnv::strtol())
                .value_or(0);
        if (tokens[path_block->row["Instance Location"]] == "-") {
          path_block->pin_obj = std::make_shared<Pin>(pin);
          Net net;
          if (path_block->row.contains("Net")) {
            net.name = std::string(tokens[path_block->row["Net"]]);
          }
          net.pins = std::make_pair(path_block->pin_obj, nullptr);
          path_block->net_obj = std::make_shared<Net>(net);
          path_block->pin_obj->net = path_block->net_obj;
          path_block->path_obj->path.push_back(path_block->pin_obj);
          return;
        }
        pin.rise_fall = tokens[path_block->row["Edge"]] == "^";
        pin.cell = std::string(tokens[path_block->row["Cell"]]);
        pin.incr_delay =
            boost::convert<double>(tokens[path_block->row["Delay"]],
                                   boost::cnv::strtol())
                .value_or(0);
        auto space_index =
            tokens[path_block->row["Instance Location"]].find(' ');
        if (space_index != std::string::npos) {
          pin.location = std::make_pair(
              boost::convert<double>(
                  tokens[path_block->row["Instance Location"]].substr(
                      1, space_index - 2),
                  boost::cnv::strtol())
                  .value_or(0),
              boost::convert<double>(
                  tokens[path_block->row["Instance Location"]].substr(
                      space_index + 1,
                      tokens[path_block->row["Instance Location"]].size() -
                          space_index - 2),
                  boost::cnv::strtol())
                  .value_or(0));
        }
        path_block->pin_obj = std::make_shared<Pin>(pin);
        if (path_block->net_obj->pins.second == nullptr) {
          path_block->net_obj->pins.second = path_block->pin_obj;
          path_block->net_obj->fanout =
              boost::convert<int>(tokens[path_block->row["Fanout"]],
                                  boost::cnv::strtol())
                  .value_or(0);
          path_block->net_obj->cap =
              boost::convert<double>(tokens[path_block->row["Load"]],
                                     boost::cnv::strtol())
                  .value_or(0);
          path_block->pin_obj->net = path_block->net_obj;
          path_block->path_obj->path.push_back(path_block->pin_obj);
        } else {
          Net net;
          if (path_block->row.contains("Net")) {
            net.name = std::string(tokens[path_block->row["Net"]]);
          }
          net.pins = std::make_pair(path_block->pin_obj, nullptr);
          path_block->net_obj = std::make_shared<Net>(net);
          path_block->pin_obj->net = path_block->net_obj;
          path_block->path_obj->path.push_back(path_block->pin_obj);
        }
      } else if (path_block->split_count == 3) {
        update_iter(path_block->iter);
      }
      break;
    default:
      break;
  }
}
