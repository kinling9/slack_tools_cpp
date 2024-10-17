#pragma once
#include <absl/container/flat_hash_map.h>
#include <re2/re2.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

class def_parser {
 public:
  const absl::flat_hash_map<std::string, std::string> get_map() const {
    return _map;
  }
  bool parse_file(const std::string &filename);
  void parse(std::istream &instream);
  void data_preparation(std::istream &instream);
  void data_processing();
  void print_paths();
  std::pair<std::string, std::string> parse_cell(const std::string &path);

 private:
  absl::flat_hash_map<std::string, std::string> _map;
  std::queue<std::string> _data_queue;
  std::mutex _data_mutex;
  std::condition_variable _data_cond_var;
  bool _done = false;  // 标志是否完成数据准备
  int _num_consumers = 4;

  const RE2 _begin_pattern{"^COMPONENTS.*"};
  const RE2 _end_pattern{"^END COMPONENTS.*"};
  const RE2 _start_pattern{"^\\s*-.*"};
  const RE2 _cell_pattern{"^\\s*-\\s+(\\S+)\\s+(\\S+).*"};
};
