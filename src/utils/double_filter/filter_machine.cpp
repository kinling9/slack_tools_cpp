#include "filter_machine.h"

std::pair<double, double> filter_machine::pop2() {
  double a = _stack.top();
  _stack.pop();
  double b = _stack.top();
  _stack.pop();
  return std::make_pair(a, b);
}

bool filter_machine::execute(const std::vector<double> &code) {
  std::vector<double>::const_iterator pc = code.begin();
  while (pc != code.end()) {
    switch (static_cast<int>(*pc++)) {
      case op_l: {
        auto [rhs, lhs] = pop2();
        _stack.push(lhs < rhs);
        break;
      }
      case op_le: {
        auto [rhs, lhs] = pop2();
        _stack.push(lhs <= rhs);
        break;
      }
      case op_g: {
        auto [rhs, lhs] = pop2();
        _stack.push(lhs > rhs);
        break;
      }
      case op_ge: {
        auto [rhs, lhs] = pop2();
        _stack.push(lhs >= rhs);
        break;
      }
      case op_eq: {
        auto [rhs, lhs] = pop2();
        _stack.push(lhs == rhs);
        break;
      }
      case op_and: {
        auto [rhs, lhs] = pop2();
        _stack.push(lhs && rhs);
        break;
      }
      case op_or: {
        auto [rhs, lhs] = pop2();
        _stack.push(lhs || rhs);
        break;
      }
      case op_not: {
        double a = _stack.top();
        _stack.pop();
        _stack.push(!a);
        break;
      }
      case op_int:
        _stack.push(*pc++);
        break;
      case op_x:
        _stack.push(_x);
        break;
    }
  }
  return static_cast<bool>(_stack.top());
}

bool slack_filter(const std::vector<double> &code, double x) {
  filter_machine vm(x);
  return vm.execute(code);
}
