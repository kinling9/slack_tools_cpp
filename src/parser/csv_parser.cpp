#include "csv_parser.h"

#include <algorithm>
#include <charconv>
#include <vector>

#include "utils/utils.h"

namespace {

// Helper to parse numbers using std::from_chars
template <typename T>
bool fast_parse(std::string_view sv, T &value) {
  if (sv.empty()) return false;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
  return ec == std::errc();
}

enum class PinCol {
  Ignore,
  Pin,
  LocX,
  LocY,
  MaxRiseSlack,
  MaxFallSlack,
  MaxRiseCap,
  MaxFallCap,
  MaxRiseTrans,
  MaxFallTrans,
  MaxRiseAt,
  MaxFallAt
};

enum class ArcCol {
  Ignore,
  FromPin,
  ToPin,
  SetupDelayRise,
  SetupDelayFall,
  Fanout
};

}  // namespace

bool csv_parser::parse_file(csv_type type, const std::string &filename) {
  CsvReaderType csv;

  if (!csv.mmap(filename)) {
    return false;
  }

  std::vector<std::string> headers;
  for (const auto &cell : csv.header()) {
    headers.push_back(std::string(cell.read_view()));
  }

  parse(type, csv, headers);
  return true;
}

void csv_parser::parse(csv_type type, CsvReaderType &csv_reader,
                       const std::vector<std::string> &headers) {
  const int max_idx = headers.size();

  if (type == csv_type::PinAT) {
    std::vector<PinCol> col_map(max_idx, PinCol::Ignore);
    auto map_col = [&](const std::string &name, PinCol col_type) {
      for (int i = 0; i < max_idx; ++i) {
        if (headers[i] == name) {
          col_map[i] = col_type;
          break;
        }
      }
    };

    map_col("pin", PinCol::Pin);
    map_col("loc_x", PinCol::LocX);
    map_col("loc_y", PinCol::LocY);
    map_col("max_rise_slack", PinCol::MaxRiseSlack);
    map_col("max_fall_slack", PinCol::MaxFallSlack);
    map_col("max_rise_cap", PinCol::MaxRiseCap);
    map_col("max_fall_cap", PinCol::MaxFallCap);
    map_col("max_rise_trans", PinCol::MaxRiseTrans);
    map_col("max_fall_trans", PinCol::MaxFallTrans);
    map_col("max_rise_at", PinCol::MaxRiseAt);
    map_col("max_fall_at", PinCol::MaxFallAt);

    for (const auto &row : csv_reader) {
      std::string pin_name;
      double x = 0.0, y = 0.0;
      double max_rise_slack = 0.0, max_fall_slack = 0.0;
      double max_rise_cap = 0.0, max_fall_cap = 0.0;
      double max_rise_trans = 0.0, max_fall_trans = 0.0;
      double max_rise_at = 0.0, max_fall_at = 0.0;

      bool has_loc_x = false;
      bool has_loc_y = false;
      bool row_valid = true;

      int idx = 0;
      for (const auto &cell : row) {
        if (idx >= max_idx) {
          idx++;
          continue;
        }

        switch (col_map[idx]) {
          case PinCol::Pin:
            pin_name = std::string(cell.read_view());
            break;
          case PinCol::LocX:
            if (fast_parse(cell.read_view(), x)) has_loc_x = true;
            break;
          case PinCol::LocY:
            if (fast_parse(cell.read_view(), y)) has_loc_y = true;
            break;
          case PinCol::MaxRiseSlack:
            if (!fast_parse(cell.read_view(), max_rise_slack))
              row_valid = false;
            break;
          case PinCol::MaxFallSlack:
            if (!fast_parse(cell.read_view(), max_fall_slack))
              row_valid = false;
            break;
          case PinCol::MaxRiseCap:
            if (!fast_parse(cell.read_view(), max_rise_cap)) row_valid = false;
            break;
          case PinCol::MaxFallCap:
            if (!fast_parse(cell.read_view(), max_fall_cap)) row_valid = false;
            break;
          case PinCol::MaxRiseTrans:
            if (!fast_parse(cell.read_view(), max_rise_trans))
              row_valid = false;
            break;
          case PinCol::MaxFallTrans:
            if (!fast_parse(cell.read_view(), max_fall_trans))
              row_valid = false;
            break;
          case PinCol::MaxRiseAt:
            if (!fast_parse(cell.read_view(), max_rise_at)) row_valid = false;
            break;
          case PinCol::MaxFallAt:
            if (!fast_parse(cell.read_view(), max_fall_at)) row_valid = false;
            break;
          default:
            break;
        }
        idx++;
      }

      if (!has_loc_x || !has_loc_y || !row_valid || pin_name.empty()) {
        continue;
      }

      double path_slack = std::min(max_rise_slack, max_fall_slack);
      Pin pin_obj{
          .name = std::move(pin_name),
          .transs = std::array<double, 2>{max_rise_trans, max_fall_trans},
          .path_delays = std::array<double, 2>{max_rise_at, max_fall_at},
          .location = {x, y},
          .caps = std::array<double, 2>{max_rise_cap, max_fall_cap},
          .path_slack = path_slack,
          .path_slacks = std::array<double, 2>{max_rise_slack, max_fall_slack},
      };
      _db.pins[pin_obj.name] = std::make_shared<Pin>(std::move(pin_obj));
    }
  } else {
    // Arc Parsing
    std::vector<ArcCol> col_map(max_idx, ArcCol::Ignore);
    auto map_col = [&](const std::string &name, ArcCol col_type) {
      for (int i = 0; i < max_idx; ++i) {
        if (headers[i] == name) {
          col_map[i] = col_type;
          break;
        }
      }
    };

    map_col("from_pin", ArcCol::FromPin);
    map_col("to_pin", ArcCol::ToPin);
    map_col("setup_delay_rise", ArcCol::SetupDelayRise);
    map_col("setup_delay_fall", ArcCol::SetupDelayFall);
    bool is_net_arc_fanout = (type == csv_type::NetArcFanout);
    if (is_net_arc_fanout) {
      map_col("fanout", ArcCol::Fanout);
    }

    bool is_cell_arc = (type == csv_type::CellArc);
    arc_type arc_type_val = is_cell_arc ? arc_type::CellArc : arc_type::NetArc;

    for (const auto &row : csv_reader) {
      std::string from_pin, to_pin;
      double setup_delay_rise = 0.0;
      double setup_delay_fall = 0.0;
      int fanout_val = 0;
      bool row_valid = true;

      int idx = 0;
      for (const auto &cell : row) {
        if (idx >= max_idx) {
          idx++;
          continue;
        }

        switch (col_map[idx]) {
          case ArcCol::FromPin:
            from_pin = std::string(cell.read_view());
            break;
          case ArcCol::ToPin:
            to_pin = std::string(cell.read_view());
            break;
          case ArcCol::SetupDelayRise:
            if (!fast_parse(cell.read_view(), setup_delay_rise))
              row_valid = false;
            break;
          case ArcCol::SetupDelayFall:
            if (!fast_parse(cell.read_view(), setup_delay_fall))
              row_valid = false;
            break;
          case ArcCol::Fanout:
            if (!fast_parse(cell.read_view(), fanout_val)) row_valid = false;
            break;
          default:
            break;
        }
        idx++;
      }

      if (!row_valid || from_pin.empty() || to_pin.empty()) {
        continue;
      }

      if (setup_delay_rise > 1e5 || setup_delay_fall > 1e5) {
        continue;
      }

      std::optional<int> fanout;
      if (is_net_arc_fanout) {
        fanout = fanout_val;
      }

      Arc arc_obj{.type = arc_type_val,
                  .from_pin = std::move(from_pin),
                  .to_pin = std::move(to_pin),
                  .delay = {setup_delay_rise, setup_delay_fall},
                  .fanout = fanout};
      _db.add_arc(is_cell_arc, std::make_shared<Arc>(std::move(arc_obj)));
    }
  }
}
