#pragma once
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "dm/dm.h"

enum block {
  Endpoint,
  Beginpoint,
  PathGroup,
  PathType,
  Slack,
  Paths,
  End,
};

class rpt_parser {
 public:
  rpt_parser(const std::string &tool_name, const std::string &start_pattern)
      : _tool_name(tool_name), _start_pattern(start_pattern) {}
  rpt_parser(const std::string &tool_name, const std::string &start_pattern,
             int num_consumers)
      : _num_consumers(num_consumers),
        _tool_name(tool_name),
        _start_pattern(start_pattern) {}
  bool parse_file(const std::string &filename);
  void parse(std::istream &instream);
  void data_preparation(std::istream &instream);
  void data_processing();
  void print_paths();
  void set_ignore(absl::flat_hash_set<block> ignore) { _ignore = ignore; }
  const basedb &get_db() const { return _db; }

  virtual std::shared_ptr<Path> parse_path(
      const std::vector<std::string> &path) = 0;
  virtual void update_iter(block &iter) = 0;

 protected:
  std::queue<std::vector<std::string>> _data_queue;
  std::mutex _data_mutex;
  std::condition_variable _data_cond_var;
  bool _done = false;  // 标志是否完成数据准备
  basedb _db;
  int _num_consumers = 4;
  std::string _tool_name;
  std::string _start_pattern;
  absl::flat_hash_set<block> _ignore;
};
