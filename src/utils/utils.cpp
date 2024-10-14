#include "utils.h"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cmath>
#include <iostream>

std::vector<std::string_view> split_string_by_spaces(
    const std::string_view &str_view, std::size_t size) {
  std::vector<std::string_view> result;
  result.reserve(size);
  std::size_t start = 0;
  while (start < str_view.size()) {
    // Find the start of the next word
    start = str_view.find_first_not_of(' ', start);
    if (start == std::string_view::npos) {
      break;
    }
    // Find the end of the word
    std::size_t end = str_view.find_first_of(' ', start);
    if (end == std::string_view::npos) end = str_view.size();
    // Add the word to the result
    result.emplace_back(str_view.substr(start, end - start));
    start = end;
  }

  return result;
}

std::vector<std::pair<std::size_t, std::string_view>> split_string_by_n_spaces(
    const std::string_view &str_view, std::size_t n, std::size_t size) {
  std::vector<std::pair<std::size_t, std::string_view>> result;
  result.reserve(size);
  std::size_t start = 0;
  std::size_t end = 0;
  std::size_t length = str_view.length();
  while (start < length) {
    // Skip over leading spaces
    start = str_view.find_first_not_of(' ', start);
    if (start == std::string_view::npos) {
      break;
    }
    // Find the end of the token
    end = str_view.find_first_of(" ", std::max(start, end));
    if (end == std::string_view::npos) {
      end = length;
    }

    // Find the start of the next token
    std::size_t next_start = str_view.find_first_not_of(" ", end);
    if (next_start == std::string_view::npos) {
      next_start = length;
    }

    // Check if there are at least n spaces after the token
    if (next_start - end >= n || end == length) {
      result.emplace_back(start, str_view.substr(start, end - start));
      start = next_start;
    } else {
      // If not n spaces, continue to the next non-space character
      end = next_start;
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
