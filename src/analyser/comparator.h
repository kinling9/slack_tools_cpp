#pragma once
#include <absl/container/flat_hash_map.h>

#include "analyser/analyser.h"
#include "dm/dm.h"

class comparator : public analyser {
 public:
  comparator(const configs &configs,
             const std::vector<std::shared_ptr<basedb>> &dbs)
      : analyser(configs), _dbs(dbs) {};
  void match();
  void gen_map();
  void analyse() override;

 private:
  std::vector<std::shared_ptr<basedb>> _dbs;
  std::vector<double> _slack_diffs;
  std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
      _path_maps;
};
