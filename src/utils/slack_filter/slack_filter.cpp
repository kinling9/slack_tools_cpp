#include "slack_filter.h"

#include <fmt/core.h>

#include "filter_compiler.h"

void compile_slack_filter(const std::string &filter_str,
                          std::vector<double> &code) {
  client::filter_compiler cmp(code);
  auto &calc = client::calculator_grammar::expression;  // Our grammar
  client::ast::expression ast_expr;

  auto iter = filter_str.begin();
  auto end = filter_str.end();
  boost::spirit::x3::ascii::space_type space;
  bool r = phrase_parse(iter, end, calc, space, ast_expr);

  if (r && iter == end) {
    fmt::print("Parsing slack_filter succeeded\n");
    cmp(ast_expr);
  } else {
    std::string rest(iter, end);
    fmt::print("Parsing slack_filter failed, stopped at \"{}\"\n", rest);
    std::exit(1);
  }
}
