#pragma once
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <list>

namespace client {
namespace ast {

namespace x3 = boost::spirit::x3;
struct nil {};
struct expression;
struct not_op;

struct operand : x3::variant<nil, double, char, x3::forward_ast<not_op>,
                             x3::forward_ast<expression>> {
  using base_type::base_type;
  using base_type::operator=;
};

struct not_op {
  std::string sign;
  operand operand_;
};

struct operation {
  std::string operator_;
  operand operand_;
};

struct expression {
  operand first;
  std::list<operation> rest;
  // operation rest;
};

}  // namespace ast
}  // namespace client

BOOST_FUSION_ADAPT_STRUCT(client::ast::not_op, sign, operand_)
BOOST_FUSION_ADAPT_STRUCT(client::ast::operation, operator_, operand_)
BOOST_FUSION_ADAPT_STRUCT(client::ast::expression, first, rest)

namespace client {
namespace calculator_grammar {
namespace x3 = boost::spirit::x3;
using x3::char_;
using x3::double_;
using x3::string;

struct value_class;
struct compare_class;
struct factor_class;
struct term_class;
struct expression_class;

x3::rule<value_class, ast::operand> const value("value");
x3::rule<compare_class, ast::expression> const compare("compare");
x3::rule<factor_class, ast::operand> const factor("factor");
x3::rule<term_class, ast::expression> const term("term");
x3::rule<expression_class, ast::expression> const expression("expression");

auto const value_def = double_ | char_('x');

auto const compare_def = value >>
                         *(string("==") >> value | string("<=") >> value |
                           string(">=") >> value | string("<") >> value |
                           string(">") >> value);

auto const factor_def = compare | "(" >> expression >> ")" |
                        (string("!") >> "(" >> expression >> ")");

auto const term_def = factor >> *((string("&&") >> factor));
auto const expression_def = term >> *((string("||") >> term));

BOOST_SPIRIT_DEFINE(value, compare, factor, term, expression);

}  // namespace calculator_grammar
}  // namespace client

void compile_double_filter(const std::string &filter_str,
                           std::vector<double> &code);
