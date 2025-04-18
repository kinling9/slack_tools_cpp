#pragma once
#include <absl/container/flat_hash_map.h>
#include <fmt/ranges.h>
#include <re2/re2.h>

#include <boost/algorithm/string.hpp>
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
  const RE2 _param_pattern{"^[-+=]?\\s*([A-Za-z ]+)\\s+([0-9\\.-]+)"};
  absl::flat_hash_map<std::size_t, std::unordered_map<std::string, std::size_t>>
      _row_cache;
  std::mutex _row_cache_mutex;
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
      iter = Params;
      break;
    case Params:
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
    case Params: {
      T key, value;
      if (RE2::PartialMatch(line, _param_pattern, &key, &value)) {
        std::string trim_key = std::string(key);
        boost::trim(trim_key);
        double data =
            boost::convert<double>(value, boost::cnv::strtol()).value_or(0);
        if (trim_key == "Slack Time") {
          path_block->path_obj->slack = data;
          update_iter(path_block->iter);
        } else if (dm::path_param_invs.contains(trim_key)) {
          auto redirect_key = dm::path_param_invs.at(trim_key);
          if (dm::path_param_invs_reverse.contains(redirect_key)) {
            path_block->path_obj->path_params[redirect_key] = -data;
          } else {
            path_block->path_obj->path_params[redirect_key] = data;
          }
        } else {
          if (trim_key == "Phase Shift" && std::abs(data) < 1e-5) {
            path_block->path_obj->path_params["data_latency"] = 0.;
          }
          path_block->path_obj->path_params[trim_key] = data;
        }
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
          auto tokens0 = split_string_by_n_spaces(path_block->headers, 2, 16);
          std::size_t key_tokens = tokens0.size();
          if (_row_cache.contains(key_tokens)) {
            path_block->row = _row_cache[key_tokens];
          } else {
            auto tokens1 = split_string_by_n_spaces(line, 2, 4);
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
            _row_cache_mutex.lock();
            _row_cache[key_tokens] = path_block->row;
            _row_cache_mutex.unlock();
          }
        }
      } else if (path_block->split_count == 2) {
        auto splits = split_string_by_n_spaces(line, 2, 16);
        std::vector<std::string_view> tokens;
        std::ranges::transform(splits, std::back_inserter(tokens),
                               [](const auto &pair) { return pair.second; });
        Pin pin;
        pin.type = "invs";
        pin.name = std::string(tokens[path_block->row["Pin"]]);
        pin.instance = std::string(tokens[path_block->row["Instance"]]);
        if (!path_block->start &&
            pin.name != path_block->path_obj->startpoint) {
          path_block->pin_obj = std::make_shared<Pin>(pin);
          return;
        }
        path_block->start = true;
        pin.trans = boost::convert<double>(tokens[path_block->row["Slew"]],
                                           boost::cnv::strtol())
                        .value_or(0);
        pin.path_delay =
            boost::convert<double>(tokens[path_block->row["Arrival Time"]],
                                   boost::cnv::strtol())
                .value_or(0);
        if (path_block->pin_obj != nullptr) {
          if (pin.instance == path_block->pin_obj->instance) {
            pin.is_input = false;
          } else {
            pin.is_input = true;
          }
        } else {
          pin.is_input = false;
        }
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
        auto loc_index = path_block->row["Instance Location"];
        auto space_index = tokens[loc_index].find(' ');
        if (space_index != std::string::npos) {
          pin.location =
              std::make_pair(boost::convert<double>(
                                 tokens[loc_index].substr(1, space_index - 2),
                                 boost::cnv::strtol())
                                 .value_or(0),
                             boost::convert<double>(
                                 tokens[loc_index].substr(
                                     space_index + 1, tokens[loc_index].size() -
                                                          space_index - 2),
                                 boost::cnv::strtol())
                                 .value_or(0));
        }
        path_block->pin_obj = std::make_shared<Pin>(pin);
        if (pin.is_input) {
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
