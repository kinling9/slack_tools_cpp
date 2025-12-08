#include "csv_parser.h"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#include "internal/csv_reader.hpp"
#include "utils/utils.h"

bool csv_parser::parse_file(csv_type type, const std::string &filename) {
  std::ifstream file(filename, std::ios_base::in | std::ios_base::binary);
  csv::CSVFormat format;
  format.trim({' ', '\t'});
  if (!isgz(filename)) {
    file.close();
    // bug in redirect stream to csv reader
    csv::CSVReader ifs(filename, format);
    parse(type, ifs);
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
    csv::CSVReader ifs(decompressed.str(), format);
    parse(type, ifs);
    file.close();
  }
  return true;
}

void csv_parser::parse(csv_type type, csv::CSVReader &ifs) {
  if (type == csv_type::PinAT) {
    for (auto &row : ifs) {
      std::string pin_name = row["pin"].get<>();
      // double x = row["loc_x"].get<double>();
      // double y = row["loc_y"].get<double>();
      std::string x_str = row["loc_x"].get<std::string>();
      std::string y_str = row["loc_y"].get<std::string>();

      // Check if strings are empty
      if (x_str.empty() || y_str.empty()) {
        continue;  // Skip this row
      }

      double x = 0, y = 0;
      // Try to convert to double
      try {
        x = std::stod(x_str);
        y = std::stod(y_str);
        // ... rest of your processing code ...
      } catch (const std::invalid_argument &e) {
        // Skip this row if conversion fails
        continue;
      } catch (const std::out_of_range &e) {
        // Skip this row if number is out of double range
        continue;
      }

      double max_rise_slack = row["max_rise_slack"].get<double>();
      double max_fall_slack = row["max_fall_slack"].get<double>();
      double path_slack = std::min(max_rise_slack, max_fall_slack);
      std::array<double, 2> path_slacks = {max_rise_slack, max_fall_slack};
      bool rf = max_rise_slack < max_fall_slack;
      double cap = rf ? row["max_rise_cap"].get<double>()
                      : row["max_fall_cap"].get<double>();
      double trans = rf ? row["max_rise_trans"].get<double>()
                        : row["max_fall_trans"].get<double>();
      double path_delay = rf ? row["max_rise_at"].get<double>()
                             : row["max_fall_at"].get<double>();
      Pin pin_obj{
          .name = pin_name,
          .trans = trans,
          .path_delay = path_delay,
          .location = {x, y},
          .cap = cap,
          .path_slack = path_slack,
          .path_slacks = path_slacks,
      };
      _db.pins[pin_name] = std::make_shared<Pin>(pin_obj);
    }
  } else {
    for (auto &row : ifs) {
      std::string from_pin = row["from_pin"].get<>();
      std::string to_pin = row["to_pin"].get<>();
      arc_type arc_type = arc_type::NetArc;
      double setup_delay_rise = row["setup_delay_rise"].get<double>();
      double setup_delay_fall = row["setup_delay_fall"].get<double>();
      if (setup_delay_rise > 1e5 || setup_delay_fall > 1e5) {
        continue;
      }
      std::optional<int> fanout;
      if (type == csv_type::NetArcFanout) {
        fanout = row["fanout"].get<int>();
      }
      if (type == csv_type::CellArc) {
        arc_type = arc_type::CellArc;
      }
      Arc arc_obj{.type = arc_type,
                  .from_pin = from_pin,
                  .to_pin = to_pin,
                  .delay = {setup_delay_rise, setup_delay_fall},
                  .fanout = fanout};
      _db.add_arc(type == csv_type::CellArc, std::make_shared<Arc>(arc_obj));
    }
  }
}
