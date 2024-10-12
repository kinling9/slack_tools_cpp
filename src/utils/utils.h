#pragma once

#include <chrono>
#include <ctime>
#include <functional>
#include <ranges>
#include <string>
#include <vector>

#include "fmt/core.h"

std::vector<std::string_view> split_string_by_spaces(
    const std::string_view &str, std::size_t size);
std::vector<std::pair<std::size_t, std::string_view>> split_string_by_n_spaces(
    const std::string_view &str, std::size_t n, std::size_t size);
bool isgz(const std::string &filename);
double variance(const std::vector<double> &arr, std::size_t n);
double standardDeviation(const std::vector<double> &arr, std::size_t n);

// Usage: run_function("name", [](){ do_something(); });
void run_function(const std::string &func_name, std::function<void()> func);

template <typename T>
T manhattan_distance(const std::vector<std::pair<T, T>> &points) {
  T distance = 0;
  for (const auto &loc_tuple : points | std::views::adjacent<2>) {
    const auto &[loc_from, loc_to] = loc_tuple;
    distance += std::abs(loc_to.first - loc_from.first) +
                std::abs(loc_to.second - loc_from.second);
  }
  return distance;
}
