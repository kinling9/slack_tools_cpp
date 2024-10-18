#pragma once
#include <absl/container/flat_hash_map.h>
#include <re2/re2.h>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

struct cell_property {
  std::string name;
  std::string cell;
  double x;
  double y;
};

namespace client {
namespace property {

struct cell_obj {
  std::string name;
  std::string cell;
  int x;
  int y;
};
}  // namespace property
}  // namespace client

BOOST_FUSION_ADAPT_STRUCT(client::property::cell_obj, name, cell, x, y)

namespace client {
namespace grammar {
namespace x3 = boost::spirit::x3;
using x3::char_;
using x3::int_;
using x3::space;

x3::rule<class word, std::string> const word("word");
x3::rule<class coordinates, property::cell_obj> const cell_obj("cell_obj");

// Define the word parser (matches characters until space)
auto const word_def = x3::lexeme[+(char_ - space)];
auto const omit_inner = x3::omit[x3::lexeme[+(x3::char_ - x3::char_("()"))]];
auto const appendix = x3::omit[x3::lexeme[+(x3::char_)]];

auto const cell_obj_def =
    "-" >> word >> word >> omit_inner >> '(' >> int_ >> int_ >> ')' >> appendix;

BOOST_SPIRIT_DEFINE(word, cell_obj)
}  // namespace grammar
}  // namespace client

class def_parser {
 public:
  const absl::flat_hash_map<std::string, std::string> get_type_map() const {
    return _type_map;
  }
  const absl::flat_hash_map<std::string, std::pair<double, double>>
  get_loc_map() const {
    return _loc_map;
  }
  bool parse_file(const std::string &filename);
  void parse(std::istream &instream);
  void data_preparation(std::istream &instream);
  void data_processing();
  void print_paths();
  cell_property parse_cell(const std::vector<std::string> &path);

 private:
  absl::flat_hash_map<std::string, std::string> _type_map;
  absl::flat_hash_map<std::string, std::pair<double, double>> _loc_map;
  std::queue<std::vector<std::string>> _data_queue;
  std::mutex _data_mutex;
  std::condition_variable _data_cond_var;
  bool _done = false;  // 标志是否完成数据准备
  int _num_consumers = 4;

  const RE2 _begin_pattern{"^COMPONENTS"};
  const RE2 _end_pattern{"^END COMPONENTS"};
  const RE2 _start_pattern{"^\\s*-"};
};
