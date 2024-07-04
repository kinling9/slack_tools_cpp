#include <re2/re2.h>

#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
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
std::mutex resultMutex;
std::condition_variable dataCondVar;
bool done = false;  // 标志是否完成数据准备

static RE2 begin_pattern("^Startpoint: (\\S*) .*");
static RE2 end_pattern("^Endpoint: (\\S*) .*");
static RE2 group_pattern("^Path Group: (\\S*)");
static RE2 path_type_pattern("^Path Type: (\\S*)");
static RE2 clock_pattern("clocked\\s+by\\s+(.*?)\\)");
static RE2 at_pattern("^data arrival time.*");

class Net;
class Pin;
class Path;
std::vector<std::shared_ptr<Path>> paths;

class Pin {
 public:
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
 public:
  std::string name;
  int fanout;
  double cap;
  std::pair<std::shared_ptr<Pin>, std::shared_ptr<Pin>> pins;
};

class Path {
 public:
  std::string startpoint;
  std::string endpoint;
  std::string group;
  std::string clock;
  std::vector<std::shared_ptr<Pin>> path;
};

class basedb {
 public:
  std::vector<std::shared_ptr<Net>> nets;
  std::vector<std::shared_ptr<Pin>> pins;
  std::vector<std::shared_ptr<Path>> paths;
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

std::shared_ptr<Path> parsePath(const std::vector<std::string> &path) {
  std::shared_ptr<Path> pathObj = std::make_shared<Path>();
  std::shared_ptr<Pin> pinObj = std::make_shared<Pin>();
  std::shared_ptr<Net> netObj = std::make_shared<Net>();
  int iter = 0;
  for (const auto &line : path) {
    switch (iter) {
      case 0:
        if (RE2::FullMatch(line, begin_pattern, &pathObj->startpoint)) {
          RE2::PartialMatch(line, clock_pattern, &pathObj->clock);
          iter++;
        }
        break;
      case 1:
        if (RE2::FullMatch(line, end_pattern, &pathObj->endpoint)) {
          iter++;
        }
        break;
      case 2:
        if (RE2::FullMatch(line, group_pattern, &pathObj->group)) {
          iter++;
        }
        break;
      case 3:
        if (RE2::FullMatch(line, path_type_pattern)) {
          iter++;
        }
        break;
      case 4: {
        if (RE2::FullMatch(line, at_pattern)) {
          iter++;
          break;
        }
        // Parse the path
        std::vector<std::string_view> tokens = splitStringBySpaces(line);
        if (tokens.size() == 8) {
          Pin pin;
          pin.cell = std::string(tokens[1].substr(1, tokens[1].size() - 2));
          pin.trans =
              boost::convert<double>(tokens[2], boost::cnv::strtol()).value();
          pin.incr_delay =
              boost::convert<double>(tokens[3], boost::cnv::strtol()).value();
          pin.path_delay =
              boost::convert<double>(tokens[4], boost::cnv::strtol()).value();
          pin.rise_fall = tokens[5] == "r";
          pin.location = std::make_pair(
              boost::convert<double>(tokens[6].substr(1, tokens[6].size() - 2),
                                     boost::cnv::strtol())
                  .value(),
              boost::convert<double>(tokens[7].substr(0, tokens[7].size() - 2),
                                     boost::cnv::strtol())
                  .value());
          pinObj = std::make_shared<Pin>(pin);
          // std::cout << pinObj->cell << "\n";
          if (netObj->pins.second == nullptr) {
            netObj->pins.second = pinObj;
            pinObj->net = netObj;
            pathObj->path.push_back(pinObj);
          }
        } else if (tokens.size() == 3) {
          Net net;
          net.name = std::string(tokens[0]);
          net.fanout =
              boost::convert<int>(tokens[1], boost::cnv::strtol()).value();
          net.cap =
              boost::convert<double>(tokens[2], boost::cnv::strtol()).value();
          net.pins = std::make_pair(pinObj, nullptr);
          netObj = std::make_shared<Net>(net);
          pinObj->net = netObj;
          pathObj->path.push_back(pinObj);
        }
      }
      default:
        break;
    }
  }
  return pathObj;
}

// 数据准备线程函数
void dataPreparation(std::istream &instream) {
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

void dataProcessing() {
  while (true) {
    std::unique_lock<std::mutex> lock(dataMutex);
    dataCondVar.wait(lock, [] { return !dataQueue.empty() || done; });

    if (!dataQueue.empty()) {
      std::vector<std::string> data = dataQueue.front();
      dataQueue.pop();
      lock.unlock();  // 解锁以便其他线程可以访问缓冲区
      std::shared_ptr<Path> path = parsePath(data);
      std::unique_lock<std::mutex> resultLock(resultMutex);
      paths.push_back(path);
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
  auto start = std::chrono::high_resolution_clock::now();
  std::ifstream file(argv[1], std::ios_base::in | std::ios_base::binary);
  boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
  inbuf.push(boost::iostreams::gzip_decompressor());
  inbuf.push(file);
  // Convert streambuf to istream
  std::istream instream(&inbuf);

  std::thread producer(dataPreparation, std::ref(instream));
  std::vector<std::thread> consumers;
  for (int i = 0; i < 4; i++) {
    consumers.emplace_back(dataProcessing);
  }
  producer.join();
  for (auto &consumer : consumers) {
    consumer.join();
  }
  file.close();
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "Elapsed time: " << elapsed.count() << " s\n";
  return 0;
}
