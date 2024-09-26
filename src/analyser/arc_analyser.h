#pragma once
#include <absl/container/flat_hash_map.h>

#include "analyser.h"
#include "dm/dm.h"
#include "utils/writer.h"

class arc_analyser : public analyser {
 public:
  arc_analyser(const configs &configs,
               const absl::flat_hash_map<
                   std::string, std::vector<std::shared_ptr<basedb>>> &dbs);

  void analyse() override;

 private:
  void gen_value_map();
  void gen_pin2path_map(
      const std::shared_ptr<basedb> &db,
      absl::flat_hash_map<std::shared_ptr<Pin>, std::shared_ptr<Path>>
          &pin2path_map);
  void match(const std::string &design,
             const absl::flat_hash_map<std::shared_ptr<Pin>,
                                       std::shared_ptr<Path>> &pin_map,
             const std::vector<std::shared_ptr<basedb>> &dbs);

 private:
  // TODO: using yml
  absl::flat_hash_map<std::string, std::vector<std::shared_ptr<basedb>>> _dbs;
  // absl::flat_hash_map<std::string, absl::flat_hash_map<std::shared_ptr<Pin>,
  //                                                      std::shared_ptr<Path>>>
  //     _pin_maps;
  absl::flat_hash_map<std::string, std::shared_ptr<writer>> _arcs_writers;
};
