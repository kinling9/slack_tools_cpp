#pragma once

#include <chrono>
#include <mutex>
#include <unordered_map>

class ScopedTimer {
 public:
  using Clock = std::chrono::high_resolution_clock;

  ScopedTimer(std::unordered_map<std::string, long long> &accum,
              const std::string &name)
      : m_accum(accum), m_name(name), m_start(Clock::now()) {}

  ~ScopedTimer() {
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
