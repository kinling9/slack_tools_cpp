#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>

#include "dm/dm.h"
#include "parser/rpt_parser.h"
#include "utils/utils.h"

template <typename T>
class leda_endpoint_parser : public rpt_parser<T> {
  using rpt_parser<T>::_ignore_blocks;
  using rpt_parser<T>::set_ignore_blocks;

 public:
  leda_endpoint_parser() : rpt_parser<T>("\\d$", Endpoint) {}
  leda_endpoint_parser(int num_consumers)
      : rpt_parser<T>("\\d$", num_consumers, Endpoint) {}
  [[maybe_unused]] void update_iter([[maybe_unused]] block &iter) override {};
  void parse_line(T line, std::shared_ptr<data_block> &path_block) override;

 private:
};

template <typename T>
void leda_endpoint_parser<T>::parse_line(
    T line, std::shared_ptr<data_block> &path_block) {
  std::vector<std::string_view> tokens = split_string_by_spaces(line);
  path_block->path_obj->slack =
      boost::convert<double>(tokens[1], boost::cnv::strtol()).value_or(0);
  path_block->path_obj->endpoint = tokens[0];
}
