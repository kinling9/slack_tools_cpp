#pragma once
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "dm/dm.h"
#include "re2/re2.h"
#include "utils/utils.h"

enum block {
  Endpoint,
  Beginpoint,
  PathGroup,
  PathType,
  Slack,
  Paths,
  End,
};

struct data_block {
  std::shared_ptr<Path> path_obj = std::make_shared<Path>();
  std::shared_ptr<Pin> pin_obj = std::make_shared<Pin>();
  std::shared_ptr<Net> net_obj = std::make_shared<Net>();
  block iter = Beginpoint;
  bool is_input = true;

  // invs
  int split_count = 0;
  std::unordered_map<std::string, std::size_t> row;
  std::string headers;

  data_block(block start_iter = Beginpoint) : iter(start_iter) {}
};

template <typename T>
class rpt_parser {
 public:
  rpt_parser(const std::string &start_pattern, const block &start_block)
      : _start_pattern(start_pattern), _start_block(start_block) {}
  rpt_parser(const std::string &start_pattern, int num_consumers,
             const block &start_block)
      : _num_consumers(num_consumers),
        _start_pattern(start_pattern),
        _start_block(start_block) {}
  bool parse_file(const std::string &filename);
  void parse(std::istream &instream);
  void data_preparation(std::istream &instream);
  void data_processing();
  void single_thread_parse(std::istream &instream);
  std::shared_ptr<Path> parse_path(const std::vector<T> &path);
  void print_paths();
  void set_ignore_blocks(absl::flat_hash_set<block> ignore_blocks) {
    _ignore_blocks = ignore_blocks;
  }
  const basedb &get_db() const { return _db; }

  virtual void parse_line(T line, std::shared_ptr<data_block> &path_block) = 0;
  virtual void update_iter(block &iter) = 0;

 protected:
  std::queue<std::vector<T>> _data_queue;
  std::mutex _data_mutex;
  std::condition_variable _data_cond_var;
  bool _done = false;  // 标志是否完成数据准备
  basedb _db;
  int _num_consumers = 4;
  std::string _start_pattern;
  absl::flat_hash_set<block> _ignore_blocks;
  block _start_block;
};

template <typename T>
bool rpt_parser<T>::parse_file(const std::string &filename) {
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

template <typename T>
void rpt_parser<T>::single_thread_parse(std::istream &instream) {
  std::string line;
  const RE2 start_pattern(_start_pattern);
  bool start_flag = false;
  std::shared_ptr<data_block> path_block =
      std::make_shared<data_block>(_start_block);
  path_block->iter = End;
  while (std::getline(instream, line)) {
    if (path_block->iter == End && RE2::PartialMatch(line, start_pattern)) {
      if (start_flag) {
        _db.paths.emplace_back(path_block->path_obj);
      }
      path_block = std::make_shared<data_block>(_start_block);
      start_flag = true;
    }
    if (start_flag) {
      parse_line(line, path_block);
    }
  }
  _db.paths.emplace_back(path_block->path_obj);
}

// 数据准备线程函数
template <typename T>
void rpt_parser<T>::data_preparation(std::istream &instream) {
  std::string line;
  const RE2 start_pattern(_start_pattern);
  std::vector<T> path;
  bool start_flag = false;
  while (std::getline(instream, line)) {
    if (RE2::PartialMatch(line, start_pattern)) {
      if (start_flag) {
        std::lock_guard<std::mutex> lock(_data_mutex);
        _data_queue.emplace(path);
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

template <typename T>
void rpt_parser<T>::data_processing() {
  std::vector<T> path;
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

template <typename T>
void rpt_parser<T>::parse(std::istream &instream) {
  if (_num_consumers == 1) {
    single_thread_parse(instream);
  } else {
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
  std::sort(_db.paths.begin(), _db.paths.end(),
            [](const std::shared_ptr<Path> &a, const std::shared_ptr<Path> &b) {
              return a->slack < b->slack;
            });
}

template <typename T>
std::shared_ptr<Path> rpt_parser<T>::parse_path(const std::vector<T> &path) {
  std::shared_ptr<data_block> path_block =
      std::make_shared<data_block>(_start_block);
  for (const T &line : path) {
    parse_line(line, path_block);
  }
  return path_block->path_obj;
}

template <typename T>
void rpt_parser<T>::print_paths() {
  for (const auto &path : _db.paths) {
    std::cout << "Startpoint: " << path->startpoint
              << " Endpoint: " << path->endpoint << " Group: " << path->group
              << " Clock: " << path->clock << " Slack: " << path->slack << "\n";
  }
}
