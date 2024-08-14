#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <fstream>
#include <thread>

#include "parser/parser.h"
#include "re2/re2.h"
#include "utils/utils.h"

bool parser::parse_file(const std::string &filename) {
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
void parser::data_preparation(std::istream &instream) {
  std::string line;
  std::vector<std::string> data;
  RE2 start_pattern(_start_pattern);
  std::vector<std::string> path;
  bool start_flag = false;
  while (std::getline(instream, line)) {
    if (RE2::FullMatch(line, start_pattern)) {
      if (start_flag) {
        std::lock_guard<std::mutex> lock(_data_mutex);
        _data_queue.push(path);
        _data_cond_var.notify_one();  // 通知等待的数据处理线程
        path.clear();
      } else {
        path.clear();
      }
      start_flag = true;
    }
    path.emplace_back(line);
  }
  std::lock_guard<std::mutex> lock(_data_mutex);
  _data_queue.push(path);
  _done = true;
  _data_cond_var.notify_all();  // 通知可能在等待的处理线程
}
void parser::data_processing() {
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
    auto path_obj = parse_path(path);
    lock.lock();
    _db.paths.emplace_back(path_obj);
    lock.unlock();
  }
}

void parser::parse(std::istream &instream) {
  std::thread producer([this, &instream] { data_preparation(instream); });
  std::vector<std::thread> consumers;
  for (int i = 0; i < 4; i++) {
    consumers.emplace_back([this] { data_processing(); });
  }
  producer.join();
  for (auto &consumer : consumers) {
    consumer.join();
  }
  std::sort(_db.paths.begin(), _db.paths.end(),
            [](const std::shared_ptr<Path> &a, const std::shared_ptr<Path> &b) {
              return a->slack < b->slack;
            });
}

void parser::print_paths() {
  for (const auto &path : _db.paths) {
    std::cout << "Startpoint: " << path->startpoint
              << " Endpoint: " << path->endpoint << " Group: " << path->group
              << " Clock: " << path->clock << " Slack: " << path->slack << "\n";
  }
}
