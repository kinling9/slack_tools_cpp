#include "filter_compiler.h"

#include <fmt/core.h>

#include "filter_machine.h"

namespace client {
void filter_compiler::operator()(double d) const {
  code.push_back(op_int);
  code.push_back(d);
}
void filter_compiler::operator()(char x) const { code.push_back(op_x); }

void filter_compiler::operator()(ast::operation const &op) const {
  boost::apply_visitor(*this, op.operand_);
  if (op.operator_ == "<")
    code.push_back(op_l);
  else if (op.operator_ == "<=")
    code.push_back(op_le);
  else if (op.operator_ == ">")
    code.push_back(op_g);
  else if (op.operator_ == ">=")
    code.push_back(op_ge);
  else if (op.operator_ == "==")
    code.push_back(op_eq);
  else if (op.operator_ == "&&")
    code.push_back(op_and);
  else if (op.operator_ == "||")
    code.push_back(op_or);
}

void filter_compiler::operator()(ast::not_op const &op) const {
  boost::apply_visitor(*this, op.operand_);
  code.push_back(op_not);
}

void filter_compiler::operator()(ast::expression const &e) const {
  boost::apply_visitor(*this, e.first);
  for (auto const &op : e.rest) {
    (*this)(op);
  }
}
}  // namespace client
