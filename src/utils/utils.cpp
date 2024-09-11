#include "utils.h"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cmath>
#include <iostream>

std::vector<std::string_view> split_string_by_spaces(const std::string &str) {
  std::vector<std::string_view> result;
  std::string_view str_view = str;
  size_t start = 0;
  size_t end = 0;

  while (end < str_view.size()) {
    // Find the start of the next word
    while (start < str_view.size() && std::isspace(str_view[start])) {
      ++start;
    }

    // Find the end of the word
    end = start;
    while (end < str_view.size() && !std::isspace(str_view[end])) {
      ++end;
    }

    // If start is less than end, we have a word
    if (start < end) {
      result.emplace_back(str_view.substr(start, end - start));
      start = end;
    }
  }

  return result;
}

bool isgz(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    fmt::print("Cannot open file {}\n", filename);
    return false;
  }

  // Read the first two bytes
  unsigned char byte1, byte2;
  file.read(reinterpret_cast<char *>(&byte1), 1);
  file.read(reinterpret_cast<char *>(&byte2), 1);

  // Check the magic numbers
  if (byte1 == 0x1F && byte2 == 0x8B) {
    return true;
  }

  return false;
}
double variance(const std::vector<double> &arr, std::size_t n) {
  // Compute mean (average of elements)
  double sum = 0;
  for (std::size_t i = 0; i < n; i++) {
    sum += arr[i];
  };
  double mean = sum / (double)n;

  // Compute sum squared
  // differences with mean.
  double sqDiff = 0;
  for (std::size_t i = 0; i < n; i++) {
    sqDiff += (arr[i] - mean) * (arr[i] - mean);
  }
  return sqDiff / n;
}

double standardDeviation(const std::vector<double> &arr, std::size_t n) {
  return std::sqrt(variance(arr, n));
}

void run_function(const std::string &func_name, std::function<void()> func) {
  auto start_elapsed = std::chrono::high_resolution_clock::now();
  std::clock_t start_cpu = std::clock();

  // Call the function with provided arguments
  func();

  std::clock_t end_cpu = std::clock();
  auto end_elapsed = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed_seconds = end_elapsed - start_elapsed;
  double cpu_seconds = double(end_cpu - start_cpu) / CLOCKS_PER_SEC;

  // Print the results
  fmt::print("Finish {}, Elapsed time: {} seconds, CPU time: {} seconds\n",
             func_name, elapsed_seconds.count(), cpu_seconds);
}
