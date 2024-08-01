#pragma once
#include "flow/configs.h"

class analyser {
 public:
  analyser(const configs &configs) : _configs(configs) {}
  virtual void analyse() = 0;

 protected:
  configs _configs;
};
