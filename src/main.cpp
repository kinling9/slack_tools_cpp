#include <re2/re2.h>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// 数据缓冲区
std::queue<std::vector<std::string>> dataQueue;
std::mutex dataMutex;
std::condition_variable dataCondVar;
bool done = false;  // 标志是否完成数据准备

class Net;
class Pin;
class Path;

class Pin {
  std::string name;
  std::string cell;
  double trans;
  double incr_delay;
  double path_delay;
  bool rise_fall;
  std::pair<double, double> location;
  std::shared_ptr<Net> net;
};

class Net {
  std::string name;
  int fanout;
  double cap;
  std::pair<std::shared_ptr<Pin>, std::shared_ptr<Pin>> pins;
};

class Path {
  std::string startpoint;
  std::string endpoint;
  std::string group;
  std::string clock;
  std::vector<Pin> path;
};

// Function to split a string by one or more spaces
std::vector<std::string_view> splitStringBySpaces(const std::string &str) {
  std::vector<std::string_view> result;
  std::string_view strView = str;
  size_t start = 0;
  size_t end = 0;

  while (end < strView.size()) {
    // Find the start of the next word
    while (start < strView.size() && std::isspace(strView[start])) {
      ++start;
    }

    // Find the end of the word
    end = start;
    while (end < strView.size() && !std::isspace(strView[end])) {
      ++end;
    }

    // If start is less than end, we have a word
    if (start < end) {
      result.emplace_back(strView.substr(start, end - start));
      start = end;
    }
  }

  return result;
}

// 数据准备线程函数
void dataPreparation(std::istream &instream) {
  auto start = std::chrono::high_resolution_clock::now();
  std::string line;
  std::vector<std::string> data;
  RE2 start_pattern("^Startpoint: .*");
  std::vector<std::string> path;
  bool start_flag = false;
  while (std::getline(instream, line)) {
    // std::cout << line << "\n";
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
  dataCondVar.notify_one();  // 通知可能在等待的处理线程
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "Elapsed time: " << elapsed.count() << " s\n";
}

void dataProcessing() {
  while (true) {
    std::unique_lock<std::mutex> lock(dataMutex);
    dataCondVar.wait(lock, [] { return !dataQueue.empty() || done; });

    if (!dataQueue.empty()) {
      std::vector<std::string> data = dataQueue.front();

      RE2 begin_pattern("^Startpoint: (\\S*)");
      RE2 end_pattern("^Endpoint: (\\S*)");
      RE2 group_pattern("^Path Group: (\\S*)");
      RE2 clock_pattern("clocked\\s+by\\s+(.*?)\\)");
      dataQueue.pop();
      lock.unlock();  // 解锁以便其他线程可以访问缓冲区
    } else if (done) {
      break;  // 如果完成数据准备并且缓冲区为空，则退出
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <gzipped input file>" << std::endl;
    return 0;
  }
  // Read from the first command line argument, assume it's gzipped
  std::ifstream file(argv[1], std::ios_base::in | std::ios_base::binary);
  boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
  inbuf.push(boost::iostreams::gzip_decompressor());
  inbuf.push(file);
  // Convert streambuf to istream
  std::istream instream(&inbuf);

  std::thread producer(dataPreparation, std::ref(instream));
  std::thread consumer(dataProcessing);
  producer.join();
  consumer.join();
  file.close();
  return 0;
}
