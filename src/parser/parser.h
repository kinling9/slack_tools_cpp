#pragma once
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "dm/dm.h"

class parser {
 public:
  void parse_file(const std::string &filename);
  void parse(std::istream &instream);
  void data_preparation(std::istream &instream);
  void data_processing();
  void print_paths();
  const basedb &get_db() const { return _db; }

  virtual void parse_path(const std::vector<std::string> &path) = 0;

 protected:
  std::queue<std::vector<std::string>> dataQueue;
  std::mutex dataMutex;
  std::mutex resultMutex;
  std::condition_variable dataCondVar;
  bool done = false;  // 标志是否完成数据准备
  std::vector<std::shared_ptr<Path>> paths_;
  basedb _db;
};
