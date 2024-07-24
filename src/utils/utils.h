#pragma once
#include <string>
#include <vector>

struct configs {
  std::string compare_mode;
};
std::vector<std::string_view> split_string_by_spaces(const std::string &str);
bool isgz(const std::string &filename);
double variance(const std::vector<double> &arr, std::size_t n);
double standardDeviation(const std::vector<double> &arr, std::size_t n);
