#pragma once

#include <nlohmann/json.hpp>

#include "dm/dm.h"

namespace rf_checker {
class rf_checker {
 public:
  rf_checker() : _enable_rise_fall(false) {}
  rf_checker(const bool& enable_rise_fall)
      : _enable_rise_fall(enable_rise_fall) {}

  void set_enable_rise_fall(const bool& enable_rise_fall) {
    _enable_rise_fall = enable_rise_fall;
  }
  bool check(const bool& rf) { return rf && _enable_rise_fall; }

 private:
  bool _enable_rise_fall;
};
}  // namespace rf_checker
