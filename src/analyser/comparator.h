#pragma once
#include "analyser/analyser.h"
#include "dm/dm.h"

class comparator : public analyser {
 public:
  comparator(const configs &configs,
             const std::vector<std::shared_ptr<basedb>> &dbs)
      : analyser(configs), _dbs(dbs) {};
  void match();

 private:
  std::vector<std::shared_ptr<basedb>> _dbs;
  std::vector<double> slack_diffs;
  const std::vector<double> _slack_margins = {0.01, 0.03, 0.05, 0.1};
  const std::vector<double> _match_percentages = {0.01, 0.03, 0.1, 0.5, 1};
};
