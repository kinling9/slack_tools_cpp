#pragma once
#include "double_filter.h"

namespace client {
struct filter_compiler {
  std::vector<double> &code;
  filter_compiler(std::vector<double> &code) : code(code) {}
  void operator()(ast::nil) const {}
  void operator()(double d) const;
  void operator()([[maybe_unused]] char _) const;
  void operator()(ast::operation const &op) const;
  void operator()(ast::not_op const &op) const;
  void operator()(ast::expression const &e) const;
};
}  // namespace client
