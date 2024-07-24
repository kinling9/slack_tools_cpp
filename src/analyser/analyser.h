#pragma once
#include "utils/utils.h"

class analyser {
 public:
  analyser(const configs &configs) : _configs(configs) {}

 protected:
  configs _configs;
};
