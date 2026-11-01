#pragma once
#include "cores/DataCacheCore.h"
class CServiceBroker
{
public:
  static CDataCacheCore& GetDataCacheCore()
  {
    static CDataCacheCore cache;
    return cache;
  }
};
