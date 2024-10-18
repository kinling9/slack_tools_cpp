#pragma once
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "analyser.h"
#include "dm/dm.h"
#include "utils/writer.h"

class path_analyser : public analyser {
 public:
  path_analyser(const configs &configs,
                const absl::flat_hash_map<
                    std::string, std::vector<std::shared_ptr<basedb>>> &dbs);

  void analyse() override;

 private:
};
