#pragma once
#include <absl/container/flat_hash_map.h>

#include "analyser/analyser.h"
#include "dm/dm.h"
#include "utils/mbff_pattern.h"

class comparator : public analyser {
 public:
  comparator(const configs &configs,
             const absl::flat_hash_map<
                 std::string, std::vector<std::shared_ptr<basedb>>> &dbs)
      : analyser(configs), _dbs(dbs) {};
  void match(
      const std::string &design,
      const std::vector<absl::flat_hash_map<std::string, std::shared_ptr<Path>>>
          &path_maps,
      const std::vector<std::shared_ptr<basedb>> &dbs);
  void gen_map(
      const std::string &tool, std::ranges::input_range auto &&paths,
      absl::flat_hash_map<std::string, std::shared_ptr<Path>> &path_map);
  void gen_headers();
  void analyse() override;

 private:
  absl::flat_hash_map<std::string, std::vector<std::shared_ptr<basedb>>> _dbs;
  std::vector<std::string> _headers;
  mbff_pattern _mbff;
};
