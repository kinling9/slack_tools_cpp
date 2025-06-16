#pragma once
#include "IRepository.h"
class IDataAccessor {
 public:
  virtual ~IDataAccessor() = default;

  std::unique_ptr<IRepository> _repository;
};
