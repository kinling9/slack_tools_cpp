#pragma once
#include <absl/strings/match.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <re2/re2.h>

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>
#include <string>
#include <vector>

#include "parser/rpt_parser.h"
#include "utils/utils.h"

template <typename T>
std::optional<T> get_param(
    const std::vector<std::string_view> &tokens, const std::string &key,
    const std::unordered_map<std::string, std::size_t> &row) {
  if (row.contains(key)) {
    int index = row.at(key);
    index = index < 0 ? -index : index;
    if (static_cast<size_t>(index) >= tokens.size()) {
      return std::nullopt;
    }
    auto token = tokens[index];
    return std::optional<T>(
        boost::convert<T>(token, boost::cnv::strtol()).value_or(T{}));
  }
  return std::nullopt;
}

void get_path_dly(const std::vector<std::string_view> &tokens,
                  const std::unordered_map<std::string, std::size_t> &row,
                  Pin &pin);

void get_location(const std::vector<std::string_view> &tokens,
                  const std::unordered_map<std::string, std::size_t> &row,
                  Pin &pin);

void get_name(const std::vector<std::string_view> &tokens,
              const std::unordered_map<std::string, std::size_t> &row,
              Pin &pin);

void get_net_name(const std::vector<std::string_view> &tokens,
                  const std::unordered_map<std::string, std::size_t> &row,
                  std::shared_ptr<Net> &net);

void get_params_from_line(
    const std::vector<std::string_view> &tokens,
    const std::unordered_map<std::string, std::string> &keys,
    std::shared_ptr<Path> &path);

template <typename T>
class leda_rpt_parser : public rpt_parser<T> {
 public:
  leda_rpt_parser() : rpt_parser<T>("^Startpoint:", Beginpoint) {}
  leda_rpt_parser(int num_consumers)
      : rpt_parser<T>("^Startpoint:", num_consumers, Beginpoint) {}
  void update_iter(block &iter) override;
  void parse_line(T line, std::shared_ptr<data_block> &path_block) override;

 protected:
  using rpt_parser<T>::_ignore_blocks;
  using rpt_parser<T>::_period;
  const RE2 _split_pattern{"^-"};
  const RE2 _at_pattern{"^data arrival time"};
  const RE2 _rat_pattern{"^data required time"};
  const RE2 _begin_pattern{"^Startpoint: (\\S*)"};
  const RE2 _end_pattern{"^Endpoint: (\\S*)"};
  const RE2 _group_pattern{"^Path Group: (\\S*)$"};
  const RE2 _path_type_pattern{"^Path Type: (\\S*)$"};
  const RE2 _clock_pattern{"clocked\\s+by\\s+(.*?)\\)$"};
  const RE2 _slack_pattern{"^slack \\(\\S+\\)\\s+([0-9.-]*)"};
  const std::unordered_map<std::string, bool> _row_type{
      {"Fanout", false},    {"Cap", false},     {"CX-Derate", false},
      {"CG-Derate", false}, {"Trans", true},    {"Incr", true},
      {"Path", true},       {"Location", true}, {"Point", true},
      {"Derate", true},     {"PtaBuf", true},   {"PtaNet", true}};
  absl::flat_hash_map<std::size_t, std::unordered_map<std::string, std::size_t>>
      _row_cache;
  absl::flat_hash_map<std::size_t, std::tuple<std::size_t, std::size_t>>
      _size_cache;
  std::mutex _row_cache_mutex;
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
      iter = Clock;
      break;
    case Clock:
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
      if (RE2::PartialMatch(line, _split_pattern)) {
        ++path_block->split_count;
        return;
      }
      if (path_block->split_count == 0) {
        auto tokens = split_string_by_n_spaces(line, 2, 8);
        std::size_t key_tokens = tokens.size();
        _row_cache_mutex.lock();
        bool contains = _row_cache.contains(key_tokens);
        _row_cache_mutex.unlock();
        if (contains) {
          std::lock_guard<std::mutex> lock(_row_cache_mutex);
          path_block->row = _row_cache[key_tokens];
          path_block->index_size = _size_cache[key_tokens];
        } else {
          int i = 0, j = -1;
          for (const auto &[_, str] : tokens) {
            std::string key = std::string(str);
            if (key == "PtaBuf") {
              path_block->is_leda_pta = true;
            }
            if (_row_type.contains(key)) {
              if (_row_type.at(key)) {
                path_block->row[key] = i++;
              } else {
                path_block->row[key] = j--;
              }
            } else {
              throw std::system_error(errno, std::generic_category(),
                                      fmt::format(fmt::fg(fmt::color::red),
                                                  "invalid row key: {}", str));
              std::exit(1);
            }
          }
          path_block->index_size = std::make_tuple(i, -j);
          _row_cache_mutex.lock();
          _row_cache[key_tokens] = path_block->row;
          _size_cache[key_tokens] = std::make_tuple(i, -j);
          _row_cache_mutex.unlock();
        }
      } else if (path_block->split_count == 1) {
        // Parse the path
        auto splits = split_string_by_n_spaces(line, 2, 8);
        std::vector<std::string_view> tokens;
        std::ranges::transform(splits, std::back_inserter(tokens),
                               [](const auto &pair) { return pair.second; });
        std::string pre_token;
        if (tokens.size() == 1) {
          path_block->headers = std::string(tokens[0]);
          break;
        } else if (!path_block->headers.empty()) {
          pre_token = path_block->headers;
          tokens.emplace(tokens.begin(), pre_token);
          path_block->headers.clear();
        }
        // conditions when token size of net & cell are the same
        const auto token_count = tokens.size();
        const auto expected_count = std::get<0>(path_block->index_size);
        const bool is_suitable_token_count =
            token_count == expected_count ||
            (path_block->is_leda_pta && token_count == expected_count - 2);
        const bool is_not_net = !absl::StrContains(tokens[0], "(net)");

        if (is_suitable_token_count && is_not_net) {
          Pin pin;
          pin.type = "leda";
          pin.is_input = path_block->is_input;
          get_name(tokens, path_block->row, pin);
          if (!path_block->start &&
              pin.name != path_block->path_obj->startpoint) {
            path_block->pin_obj = std::make_shared<Pin>(pin);
            return;
          }
          path_block->start = true;
          pin.trans =
              get_param<double>(tokens, "Trans", path_block->row).value_or(0);
          pin.incr_delay = get_param<double>(tokens, "Incr", path_block->row);
          if (path_block->is_leda_pta) {
            // double pta_buf = 0, pta_net = 0;
            pin.pta_buf = get_param<double>(tokens, "PtaBuf", path_block->row);
            pin.pta_net = get_param<double>(tokens, "PtaNet", path_block->row);
            // if (token_count == expected_count) {
            //   pin.pta_buf = pta_buf;
            //   pin.pta_net = pta_net;
            // }
          }
          get_path_dly(tokens, path_block->row, pin);
          if (pin.name == path_block->path_obj->startpoint &&
              !path_block->path_obj->path_params.contains(
                  "input_external_delay")) {
            path_block->path_obj->path_params["data_latency"] = pin.path_delay;
          }
          get_location(tokens, path_block->row, pin);
          path_block->pin_obj = std::make_shared<Pin>(pin);
          if (path_block->net_obj->pins.second == nullptr) {
            path_block->net_obj->pins.second = path_block->pin_obj;
            path_block->pin_obj->net = path_block->net_obj;
            path_block->path_obj->path.push_back(path_block->pin_obj);
          }
        } else if (tokens.size() == std::get<1>(path_block->index_size) &&
                   absl::StrContains(tokens[0], "(net)")) {
          if (!path_block->start) {
            return;
          }
          bool push = true;
          if (path_block->net_obj->pins.first == nullptr &&
              path_block->net_obj->pins.second == path_block->pin_obj) {
            push = false;
          } else {
            path_block->net_obj = std::make_shared<Net>();
          }
          auto &net = path_block->net_obj;
          get_net_name(tokens, path_block->row, net);
          net->fanout =
              get_param<int>(tokens, "Fanout", path_block->row).value_or(0);
          net->cap =
              get_param<double>(tokens, "Cap", path_block->row).value_or(0.);
          path_block->pin_obj->is_input = false;
          net->pins = std::make_pair(path_block->pin_obj, nullptr);
          path_block->pin_obj->net = net;
          if (push) {
            path_block->path_obj->path.push_back(path_block->pin_obj);
          }
          path_block->is_input = true;
        } else if (tokens.size() == 3) {
          get_params_from_line(
              tokens,
              {{"input external delay", "input_external_delay"},
               {"clock offset latency", "data_latency"}},
              path_block->path_obj);
        }
      }
      break;
    }
    case Clock: {
      if (RE2::PartialMatch(line, _rat_pattern)) {
        update_iter(path_block->iter);
        break;
      }
      auto splits = split_string_by_n_spaces(line, 2, 8);

      std::vector<std::string_view> tokens;
      std::ranges::transform(splits, std::back_inserter(tokens),
                             [](const auto &pair) { return pair.second; });
      if (tokens.size() == 3) {
        get_params_from_line(
            tokens,
            {
                {"clock uncertainty", "clock_uncertainty"},
                {"output external delay", "output_external_delay"},
                {"clock offset latency", "clock_latency"},
                {"library recovery time", "library_setup_time"},
                {"library setup time", "library_setup_time"},
            },
            path_block->path_obj);
        if (absl::StrContains(tokens[0], "clock reconvergence pessimism")) {
          auto pessimism =
              boost::convert<double>(tokens[1], boost::cnv::strtol()).value();
          auto rat =
              boost::convert<double>(tokens[2], boost::cnv::strtol()).value();
          auto value = _period - rat + pessimism;
          if (std::abs(value) > 1e-10) {
            path_block->path_obj->path_params["clock_latency"] = value;
          }
          break;
        }
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
