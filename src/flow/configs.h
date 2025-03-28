#pragma once
#include <limits>
#include <string>
#include <vector>

struct configs {
  std::string mode;
  std::string compare_mode;
  std::string output_dir = "output";
  bool enable_mbff = false;
  // std::vector<std::string> design_names;

  // default values
  std::vector<double> slack_margins = {0.01, 0.03, 0.05, 0.1};
  std::vector<double> match_percentages = {0.01, 0.03, 0.1, 0.5, 1};
  std::size_t match_paths = std::numeric_limits<std::size_t>::max();

  std::vector<double> slack_filter_op_code;
  std::vector<double> diff_filter_op_code;
  std::vector<double> delay_filter_op_code;
  std::vector<double> fanout_filter_op_code;
};
