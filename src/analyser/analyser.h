#pragma once
#include <fmt/core.h>

#include "flow/configs.h"
#include "utils/csv_writer.h"

class analyser {
 public:
  analyser(const configs &configs)
      : _configs(configs),
        _writer(configs.match_paths == 0
                    ? fmt::format("{}.csv", configs.compare_mode)
                    : fmt::format("{}_{}.csv", configs.compare_mode,
                                  configs.match_paths)) {}
  virtual void analyse() = 0;

 protected:
  configs _configs;
  csv_writer _writer;
};
