#pragma once

#include <string_view>
#include <vector>

// cache_result stores the result of a shortest path query
class cache_result {
 public:
  double distance;
  std::vector<std::string_view> path;
  cache_result(double dist, const std::vector<std::string_view> &p)
      : distance(dist), path(p) {}
  cache_result() : distance(-1), path({}) {}
};
