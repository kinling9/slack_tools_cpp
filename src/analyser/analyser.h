#pragma once
#include <fmt/core.h>

#include "flow/configs.h"
#include "utils/csv_writer.h"

class analyser {
 public:
  analyser(const configs &configs)
      : _configs(configs),
        _writer(configs.match_paths == std::numeric_limits<std::size_t>::max()
                    ? fmt::format("{}.csv", configs.compare_mode)
                    : fmt::format("{}_{}.csv", configs.compare_mode,
                                  configs.match_paths)) {
    _writer.set_output_dir(_configs.output_dir);
  }
  virtual void analyse() = 0;

 protected:
  configs _configs;
  csv_writer _writer;
};
