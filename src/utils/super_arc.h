#pragma once

#include <nlohmann/json.hpp>

#include "dm/dm.h"

namespace super_arc {
nlohmann::json to_json(const std::shared_ptr<Path> path,
                       std::pair<std::string, std::string> names);
}
