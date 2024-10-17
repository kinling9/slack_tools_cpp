#include "parser/def_parser.h"

#include <boost/iostreams/copy.hpp>
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
  std::string path;
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
    if (RE2::FullMatch(line, _start_pattern)) {
      if (start_flag) {
        std::lock_guard<std::mutex> lock(_data_mutex);
        _data_queue.push(path);
        _data_cond_var.notify_one();  // 通知等待的数据处理线程
      }
      start_flag = true;
      path = line;
    }
  }
  std::lock_guard<std::mutex> lock(_data_mutex);
  _data_queue.push(path);
  _done = true;
  _data_cond_var.notify_all();  // 通知可能在等待的处理线程
}

void def_parser::data_processing() {
  std::string path;
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
    _map.emplace(cell_obj);
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
std::pair<std::string, std::string> def_parser::parse_cell(
    const std::string &path) {
  std::string cell_name;
  std::string cell_type;
  RE2::FullMatch(path, _cell_pattern, &cell_name, &cell_type);
  return std::make_pair(cell_name, cell_type);
}
