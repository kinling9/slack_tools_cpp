#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>
#include <iostream>
#include <thread>

#include "parser/leda_rpt.h"
#include "utils/utils.h"

void leda_rpt_parser::parse_path(const std::vector<std::string> &path) {
  std::shared_ptr<Path> pathObj = std::make_shared<Path>();
  std::shared_ptr<Pin> pinObj = std::make_shared<Pin>();
  std::shared_ptr<Net> netObj = std::make_shared<Net>();
  int iter = 0;
  for (const auto &line : path) {
    switch (iter) {
      case 0:
        if (RE2::FullMatch(line, begin_pattern_, &pathObj->startpoint)) {
          RE2::PartialMatch(line, clock_pattern_, &pathObj->clock);
          iter++;
        }
        break;
      case 1:
        if (RE2::FullMatch(line, end_pattern_, &pathObj->endpoint)) {
          iter++;
        }
        break;
      case 2:
        if (RE2::FullMatch(line, group_pattern_, &pathObj->group)) {
          iter++;
        }
        break;
      case 3:
        if (RE2::FullMatch(line, path_type_pattern_)) {
          iter++;
        }
        break;
      case 4: {
        if (RE2::FullMatch(line, at_pattern_)) {
          iter++;
          break;
        }
        // Parse the path
        std::vector<std::string_view> tokens = split_string_by_spaces(line);
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
  paths_.emplace_back(pathObj);
}

// 数据准备线程函数
void leda_rpt_parser::data_preparation(std::istream &instream) {
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
void leda_rpt_parser::data_processing() {
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

void leda_rpt_parser::parse(std::istream &instream) {
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

void leda_rpt_parser::print_paths() {
  for (const auto &path : paths_) {
    std::cout << "Startpoint: " << path->startpoint
              << " Endpoint: " << path->endpoint << " Group: " << path->group
              << " Clock: " << path->clock << "\n";
  }
}
