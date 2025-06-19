#include "csv_parser.h"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#include "internal/csv_reader.hpp"
#include "utils/utils.h"

bool csv_parser::parse_file(bool is_cell_arc, const std::string &filename) {
  std::ifstream file(filename, std::ios_base::in | std::ios_base::binary);
  if (!isgz(filename)) {
    file.close();
    std::ifstream simple_file(filename);

    if (!simple_file.is_open()) {
      return false;
    }
    csv::CSVReader ifs(simple_file);
    parse(is_cell_arc, ifs);
    simple_file.close();
  } else {
    // boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
    // inbuf.push(boost::iostreams::gzip_decompressor());
    // inbuf.push(file);
    // // Convert streambuf to istream
    // std::stringstream instream(&inbuf);
    boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
    inbuf.push(boost::iostreams::gzip_decompressor());
    inbuf.push(file);
    std::ostringstream decompressed;
    boost::iostreams::copy(inbuf, decompressed);
    csv::CSVReader ifs(decompressed.str());
    parse(is_cell_arc, ifs);
    file.close();
  }
  return true;
}

void csv_parser::parse(bool is_cell_arc, csv::CSVReader &ifs) {
  for (auto &row : ifs) {
    std::string from_pin = row["from_pin"].get<>();
    std::string to_pin = row["to_pin"].get<>();
    double setup_delay_rise = row["setup_delay_rise"].get<double>();
    double setup_delay_fall = row["setup_delay_fall"].get<double>();
    Arc arc_obj{.from_pin = from_pin,
                .to_pin = to_pin,
                .delay = {setup_delay_rise, setup_delay_fall}};
    _db.add_arc(is_cell_arc, std::make_shared<Arc>(arc_obj));
  }
}
