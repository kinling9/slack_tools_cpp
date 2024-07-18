#pragma once
#include <re2/re2.h>

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "dm/dm.h"

class leda_rpt_parser {
 public:
  void parse_path(const std::vector<std::string> &path);
  void data_preparation(std::istream &instream);
  void data_processing();
  void parse(std::istream &instream);
  void print_paths();

 private:
  std::queue<std::vector<std::string>> dataQueue;
  std::mutex dataMutex;
  std::mutex resultMutex;
  std::condition_variable dataCondVar;
  bool done = false;  // 标志是否完成数据准备
  const RE2 at_pattern_{"^data arrival time.*"};
  const RE2 begin_pattern_{"^Startpoint: (\\S*) .*"};
  const RE2 end_pattern_{"^Endpoint: (\\S*) .*"};
  const RE2 group_pattern_{"^Path Group: (\\S*)"};
  const RE2 path_type_pattern_{"^Path Type: (\\S*)"};
  const RE2 clock_pattern_{"clocked\\s+by\\s+(.*?)\\)"};
  std::vector<std::shared_ptr<Path>> paths_;
};
