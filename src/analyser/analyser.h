#pragma once
#include <fmt/core.h>

#include "flow/configs.h"
#include "utils/csv_writer.h"

class analyser {
 public:
  analyser(const configs &configs)
      : _configs(configs),
        _writer(fmt::format("{}.csv", configs.compare_mode)) {}
  virtual void analyse() = 0;

 protected:
  configs _configs;
  csv_writer _writer;
};
