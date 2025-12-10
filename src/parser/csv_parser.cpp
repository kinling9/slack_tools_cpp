#include "csv_parser.h"

#include <algorithm>  // For std::min
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <stdexcept>  // For std::invalid_argument, std::out_of_range

#include "utils/utils.h"

bool csv_parser::parse_file(csv_type type, const std::string &filename) {
  CsvReaderType csv;  // Use the alias

  if (!csv.mmap(filename)) {
    // Handle error: could not open or parse CSV file
    return false;
  }

  std::map<std::string, int> header_map;
  int i = 0;
  for (const auto &cell : csv.header()) {
    std::string col_name;
    cell.read_value(col_name);
    header_map[col_name] = i++;
  }

  parse(type, csv, header_map);
  return true;
}

void csv_parser::parse(csv_type type, CsvReaderType &csv_reader,
                       const std::map<std::string, int> &header_map) {
  // Helper lambda to safely get index or -1 if missing
  auto get_idx = [&](const std::string &name) {
    auto it = header_map.find(name);
    return (it != header_map.end()) ? it->second : -1;
  };

  if (type == csv_type::PinAT) {
    // Resolve indices for PinAT
    int idx_pin = get_idx("pin");
    int idx_loc_x = get_idx("loc_x");
    int idx_loc_y = get_idx("loc_y");
    int idx_max_rise_slack = get_idx("max_rise_slack");
    int idx_max_fall_slack = get_idx("max_fall_slack");
    int idx_max_rise_cap = get_idx("max_rise_cap");
    int idx_max_fall_cap = get_idx("max_fall_cap");
    int idx_max_rise_trans = get_idx("max_rise_trans");
    int idx_max_fall_trans = get_idx("max_fall_trans");
    int idx_max_rise_at = get_idx("max_rise_at");
    int idx_max_fall_at = get_idx("max_fall_at");

    for (const auto &row : csv_reader) {
      std::string pin_name;
      std::string x_str, y_str;
      std::string max_rise_slack_str, max_fall_slack_str;
      std::string max_rise_cap_str, max_fall_cap_str;
      std::string max_rise_trans_str, max_fall_trans_str;
      std::string max_rise_at_str, max_fall_at_str;

      int idx = 0;
      for (const auto &cell : row) {
        if (idx == idx_pin)
          cell.read_value(pin_name);
        else if (idx == idx_loc_x)
          cell.read_value(x_str);
        else if (idx == idx_loc_y)
          cell.read_value(y_str);
        else if (idx == idx_max_rise_slack)
          cell.read_value(max_rise_slack_str);
        else if (idx == idx_max_fall_slack)
          cell.read_value(max_fall_slack_str);
        else if (idx == idx_max_rise_cap)
          cell.read_value(max_rise_cap_str);
        else if (idx == idx_max_fall_cap)
          cell.read_value(max_fall_cap_str);
        else if (idx == idx_max_rise_trans)
          cell.read_value(max_rise_trans_str);
        else if (idx == idx_max_fall_trans)
          cell.read_value(max_fall_trans_str);
        else if (idx == idx_max_rise_at)
          cell.read_value(max_rise_at_str);
        else if (idx == idx_max_fall_at)
          cell.read_value(max_fall_at_str);
        idx++;
      }

      if (x_str.empty() || y_str.empty()) {
        continue;
      }

      double x = 0, y = 0;
      try {
        x = std::stod(x_str);
        y = std::stod(y_str);
      } catch (const std::invalid_argument &) {
        continue;
      } catch (const std::out_of_range &) {
        continue;
      }
      double max_rise_slack = 0.0, max_fall_slack = 0.0;
      double max_rise_cap = 0.0, max_fall_cap = 0.0;
      double max_rise_trans = 0.0, max_fall_trans = 0.0;
      double max_rise_at = 0.0, max_fall_at = 0.0;
      try {
        max_rise_slack = std::stod(max_rise_slack_str);
        max_fall_slack = std::stod(max_fall_slack_str);
        max_rise_cap = std::stod(max_rise_cap_str);
        max_fall_cap = std::stod(max_fall_cap_str);
        max_rise_trans = std::stod(max_rise_trans_str);
        max_fall_trans = std::stod(max_fall_trans_str);
        max_rise_at = std::stod(max_rise_at_str);
        max_fall_at = std::stod(max_fall_at_str);
      } catch (const std::invalid_argument &) {
        continue;
      } catch (const std::out_of_range &) {
        continue;
      }

      double path_slack = std::min(max_rise_slack, max_fall_slack);
      std::array<double, 2> path_slacks = {max_rise_slack, max_fall_slack};
      std::array<double, 2> caps = {max_rise_cap, max_fall_cap};
      std::array<double, 2> transs = {max_rise_trans, max_fall_trans};
      std::array<double, 2> path_delays = {max_rise_at, max_fall_at};

      Pin pin_obj{
          .name = pin_name,
          .transs = transs,
          .path_delays = path_delays,
          .location = {x, y},
          .caps = caps,
          .path_slack = path_slack,
          .path_slacks = path_slacks,
      };
      _db.pins[pin_name] = std::make_shared<Pin>(pin_obj);
    }
  } else {
    // Resolve indices for Arcs
    int idx_from_pin = get_idx("from_pin");
    int idx_to_pin = get_idx("to_pin");
    int idx_setup_delay_rise = get_idx("setup_delay_rise");
    int idx_setup_delay_fall = get_idx("setup_delay_fall");
    int idx_fanout = (type == csv_type::NetArcFanout) ? get_idx("fanout") : -1;

    for (const auto &row : csv_reader) {
      std::string from_pin, to_pin;
      std::string setup_delay_rise_str, setup_delay_fall_str;
      std::string fanout_val_str;

      int idx = 0;
      for (const auto &cell : row) {
        if (idx == idx_from_pin)
          cell.read_value(from_pin);
        else if (idx == idx_to_pin)
          cell.read_value(to_pin);
        else if (idx == idx_setup_delay_rise)
          cell.read_value(setup_delay_rise_str);
        else if (idx == idx_setup_delay_fall)
          cell.read_value(setup_delay_fall_str);
        else if (idx_fanout != -1 && idx == idx_fanout)
          cell.read_value(fanout_val_str);
        idx++;
      }

      double setup_delay_rise = 0.0;
      double setup_delay_fall = 0.0;
      int fanout_val = 0;
      try {
        setup_delay_rise = std::stod(setup_delay_rise_str);
        setup_delay_fall = std::stod(setup_delay_fall_str);
        if (idx_fanout != -1) {
          fanout_val = std::stoi(fanout_val_str);
        }
      } catch (const std::invalid_argument &) {
        continue;
      } catch (const std::out_of_range &) {
        continue;
      }

      if (setup_delay_rise > 1e5 || setup_delay_fall > 1e5) {
        continue;
      }

      std::optional<int> fanout;
      if (type == csv_type::NetArcFanout) {
        fanout = fanout_val;
      }

      arc_type arc_type_val =
          (type == csv_type::CellArc) ? arc_type::CellArc : arc_type::NetArc;

      Arc arc_obj{.type = arc_type_val,
                  .from_pin = from_pin,
                  .to_pin = to_pin,
                  .delay = {setup_delay_rise, setup_delay_fall},
                  .fanout = fanout};
      _db.add_arc(type == csv_type::CellArc, std::make_shared<Arc>(arc_obj));
    }
  }
}
