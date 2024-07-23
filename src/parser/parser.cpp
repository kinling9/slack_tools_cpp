#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <fstream>
#include <thread>

#include "parser/parser.h"
#include "re2/re2.h"
#include "utils/utils.h"

void parser::parse_file(const std::string &filename) {
  std::ifstream file(filename, std::ios_base::in | std::ios_base::binary);
  if (!isgz(filename)) {
    file.close();
    std::ifstream simple_file(filename);
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

  print_paths();
}

// 数据准备线程函数
void parser::data_preparation(std::istream &instream) {
  std::string line;
  std::vector<std::string> data;
  RE2 start_pattern("^Startpoint: .*");
  std::vector<std::string> path;
  bool start_flag = false;
  while (std::getline(instream, line)) {
    if (RE2::FullMatch(line, start_pattern)) {
      if (start_flag) {
        std::lock_guard<std::mutex> lock(dataMutex);
        dataQueue.push(path);
        dataCondVar.notify_one();  // 通知等待的数据处理线程
        path.clear();
      } else {
        path.clear();
      }
      start_flag = true;
    }
    path.emplace_back(line);
  }
  std::lock_guard<std::mutex> lock(dataMutex);
  dataQueue.push(path);
  done = true;
  dataCondVar.notify_all();  // 通知可能在等待的处理线程
}
void parser::data_processing() {
  std::vector<std::string> path;
  while (true) {
    std::unique_lock<std::mutex> lock(dataMutex);
    dataCondVar.wait(lock, [this] { return !dataQueue.empty() || done; });
    if (dataQueue.empty() && done) {
      break;
    }
    path = dataQueue.front();
    dataQueue.pop();
    lock.unlock();
    parse_path(path);
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
}

void parser::print_paths() {
  for (const auto &path : paths_) {
    std::cout << "Startpoint: " << path->startpoint
              << " Endpoint: " << path->endpoint << " Group: " << path->group
              << " Clock: " << path->clock << "\n";
  }
}
