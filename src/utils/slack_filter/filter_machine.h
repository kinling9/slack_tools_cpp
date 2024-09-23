#pragma once
#include <stack>
#include <vector>

enum byte_code {
  op_l,
  op_le,
  op_g,
  op_ge,
  op_eq,
  op_and,
  op_or,
  op_not,
  op_int,  // push value
  op_x,    // push x
};

class filter_machine {
 public:
  filter_machine(const double x) : _x(x) {}
  bool execute(const std::vector<double> &code);

 private:
  std::pair<double, double> pop2();
  std::stack<double> _stack;  // The VM stack
  double _x;
};

bool slack_filter(const std::vector<double> &code, double x);
