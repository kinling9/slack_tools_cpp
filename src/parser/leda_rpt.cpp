#include "leda_rpt.h"
void get_path_dly(const std::vector<std::string_view> &tokens,
                  const std::unordered_map<std::string, std::size_t> &row,
                  Pin &pin) {
  if (row.contains("Path")) {
    auto token = tokens[row.at("Path")];
    auto delay_rf = split_string_by_spaces(token, 2);
    pin.path_delay =
        boost::convert<double>(delay_rf[0], boost::cnv::strtol()).value();
    pin.rise_fall = delay_rf[1] == "r";
  }
}

void get_location(const std::vector<std::string_view> &tokens,
                  const std::unordered_map<std::string, std::size_t> &row,
                  Pin &pin) {
  if (row.contains("Location")) {
    auto index = row.at("Location");
    auto space_index = tokens[index].find(" ");
    if (space_index != std::string::npos) {
      pin.location = std::make_pair(
          boost::convert<double>(tokens[index].substr(1, space_index - 2),
                                 boost::cnv::strtol())
              .value(),
          boost::convert<double>(
              tokens[index].substr(space_index + 1,
                                   tokens[index].size() - space_index - 2),
              boost::cnv::strtol())
              .value());
    }
  }
}

void get_name(const std::vector<std::string_view> &tokens,
              const std::unordered_map<std::string, std::size_t> &row,
              Pin &pin) {
  if (row.contains("Point")) {
    auto name_cell = split_string_by_spaces(tokens[row.at("Point")], 2);
    pin.name = std::string(name_cell[0]);
    pin.cell = std::string(name_cell[1].substr(1, name_cell[1].size() - 2));
  }
}

void get_net_name(const std::vector<std::string_view> &tokens,
                  const std::unordered_map<std::string, std::size_t> &row,
                  std::shared_ptr<Net> &net) {
  if (row.contains("Point")) {
    auto net_name = split_string_by_spaces(tokens[row.at("Point")], 2);
    net->name = std::string(net_name[0]);
  }
}

void get_params_from_line(
    const std::vector<std::string_view> &tokens,
    const std::unordered_map<std::string, std::string_view> &keys,
    std::shared_ptr<Path> &path) {
  for (const auto &[key, param] : keys) {
    if (absl::StrContains(tokens[0], param)) {
      auto value =
          boost::convert<double>(tokens[1], boost::cnv::strtol()).value();
      if (std::abs(value) > 1e-10) {
        path->path_params[key] = value;
      }
      break;
    }
  }
}
