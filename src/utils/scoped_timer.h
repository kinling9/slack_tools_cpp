#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

class scoped_timer {
 public:
  using Clock = std::chrono::high_resolution_clock;

  scoped_timer(std::unordered_map<std::string, long long> &accum,
               const std::string &name)
      : m_accum(accum), m_name(name), m_start(Clock::now()) {}

  ~scoped_timer() {
    auto end = Clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - m_start)
            .count();
    std::lock_guard<std::mutex> lock(get_timer_mutex());
    m_accum[m_name] += duration;
  }

  std::mutex &get_timer_mutex() {
    static std::mutex timer_mutex;
    return timer_mutex;
  }

 private:
  std::unordered_map<std::string, long long> &m_accum;
  std::string m_name;
  std::chrono::time_point<Clock> m_start;
};
