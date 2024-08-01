#pragma once
#include "flow/configs.h"

class analyser {
 public:
  analyser(const configs &configs) : _configs(configs) {}

 protected:
  configs _configs;
};
