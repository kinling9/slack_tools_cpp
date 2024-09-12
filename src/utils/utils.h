#pragma once

#include <chrono>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "fmt/core.h"

std::vector<std::string_view> split_string_by_spaces(const std::string &str);
std::vector<std::pair<std::size_t, std::string_view>> split_string_by_n_spaces(
    const std::string &str, std::size_t n);
bool isgz(const std::string &filename);
double variance(const std::vector<double> &arr, std::size_t n);
double standardDeviation(const std::vector<double> &arr, std::size_t n);

// Usage: run_function("name", [](){ do_something(); });
void run_function(const std::string &func_name, std::function<void()> func);
