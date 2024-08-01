#pragma once
#include <string>
#include <vector>

struct configs {
  std::string compare_mode;
  std::string design_name;

  // default values
  std::vector<double> slack_margins = {0.01, 0.03, 0.05, 0.1};
  std::vector<double> match_percentages = {0.01, 0.03, 0.1, 0.5, 1};
};
