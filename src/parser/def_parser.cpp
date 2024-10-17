#include "parser/def_parser.h"

#include <fmt/ranges.h>

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <fstream>
#include <thread>

#include "re2/re2.h"
#include "utils/utils.h"

bool def_parser::parse_file(const std::string &filename) {
  std::ifstream file(filename, std::ios_base::in | std::ios_base::binary);
  if (!isgz(filename)) {
    file.close();
    std::ifstream simple_file(filename);

    if (!simple_file.is_open()) {
      return false;
    }
    parse(simple_file);
    simple_file.close();
  } else {
    boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
    inbuf.push(boost::iostreams::gzip_decompressor());
    inbuf.push(file);
    // Convert streambuf to istream
    std::istream instream(&inbuf);
    parse(instream);
    file.close();
  }
  return true;
}

// 数据准备线程函数
void def_parser::data_preparation(std::istream &instream) {
  std::string line;
  std::vector<std::string> path;
  bool start_flag = false;
  bool component_flag = false;
  while (std::getline(instream, line)) {
    if (RE2::FullMatch(line, _begin_pattern)) {
      component_flag = true;
    }
    if (RE2::FullMatch(line, _end_pattern)) {
      break;
    }
    if (!component_flag) {
      continue;
    }
    if (RE2::PartialMatch(line, _start_pattern)) {
      if (start_flag) {
        std::lock_guard<std::mutex> lock(_data_mutex);
        _data_queue.push(path);
        path.clear();
        _data_cond_var.notify_one();  // 通知等待的数据处理线程
      }
      start_flag = true;
    }
    if (start_flag) {
      path.emplace_back(line);
    }
  }
  std::lock_guard<std::mutex> lock(_data_mutex);
  _data_queue.push(path);
  path.clear();
  _done = true;
  _data_cond_var.notify_all();  // 通知可能在等待的处理线程
}

void def_parser::data_processing() {
  std::vector<std::string> path;
  while (true) {
    std::unique_lock<std::mutex> lock(_data_mutex);
    _data_cond_var.wait(lock, [this] { return !_data_queue.empty() || _done; });
    if (_data_queue.empty() && _done) {
      break;
    }
    path = _data_queue.front();
    _data_queue.pop();
    lock.unlock();
    auto cell_obj = parse_cell(path);
    lock.lock();
    _type_map.emplace(cell_obj.name, cell_obj.cell);
    _loc_map.emplace(cell_obj.name, std::make_pair(cell_obj.x, cell_obj.y));
    lock.unlock();
  }
}

void def_parser::parse(std::istream &instream) {
  std::thread producer([this, &instream] { data_preparation(instream); });
  std::vector<std::thread> consumers;
  for (int i = 0; i < _num_consumers; i++) {
    consumers.emplace_back([this] { data_processing(); });
  }
  producer.join();
  for (auto &consumer : consumers) {
    consumer.join();
  }
}

cell_property def_parser::parse_cell(const std::vector<std::string> &path) {
  // cell_property cell_obj;
  std::string line = fmt::format("{}", fmt::join(path, " "));
  client::property::cell_obj cell_obj;
  auto &component_grammar = client::grammar::cell_obj;
  auto iter = line.begin();
  auto end = line.end();
  boost::spirit::x3::ascii::space_type space;
  bool r = boost::spirit::x3::phrase_parse(iter, end, component_grammar, space,
                                           cell_obj);
  if (!r || iter != end) {
    std::string rest(iter, end);
    fmt::print("Parsing def component failed, stopped at \"{}\"\n", rest);
    std::exit(1);
  }
  cell_property cell{cell_obj.name, cell_obj.cell, cell_obj.x / 2000.,
                     cell_obj.y / 2000.};

  return cell;
}
