#pragma once
#include <string>
class CDataCacheCore
{
public:
  void SetVideoFELStatus(const std::string& value) { status = value; }
  std::string status;
};
