#include "AndroidStreamMetadata.h"
#include <cassert>
#include <cstdint>
#include <vector>

std::vector<uint8_t> Nal(std::vector<uint8_t> payload)
{
  std::vector<uint8_t> out{78, 1};
  unsigned zeros=0;
  for (auto b:payload)
  {
    if (zeros>=2 && b<=3) { out.push_back(3); zeros=0; }
    out.push_back(b); zeros=b==0?zeros+1:0;
  }
  return out;
}
int main()
{
  CAndroidStreamMetadata metadata;
  // ST 2086 max 4000 nits, min .005; CLL 1000, FALL 362.
  std::vector<uint8_t> payload{137,24};
  payload.resize(18,0);
  for (auto b: {2,98,90,0,0,0,0,50,144,4,3,232,1,106,128}) payload.push_back(b);
  auto nal=Nal(payload);
  metadata.Observe(nal.data(),nal.size());
  assert(metadata.TakeChanges());
  const auto& v=metadata.Values();
  assert(v.at("video.hdr.max.lum")=="4000");
  assert(v.at("video.hdr.min.lum")=="0.0050");
  assert(v.at("video.hdr.max.cll")=="1000");
  assert(v.at("video.hdr.max.fall")=="362");
  assert(!metadata.TakeChanges());
  // Every truncated prefix must be safe; only complete metadata may appear.
  for(size_t n=0;n<nal.size();++n)
  {
    CAndroidStreamMetadata truncated;
    truncated.Observe(nal.data(),n);
  }
  CAndroidStreamMetadata enhancement;
  nal[1]=9;
  enhancement.Observe(nal.data(),nal.size());
  assert(enhancement.Values().empty());
  CAndroidStreamMetadata fresh;
  assert(fresh.Values().empty());
  const uint8_t bad[]={78,1,255,255,255,255};
  fresh.Observe(bad,sizeof(bad));
  assert(fresh.Values().empty());
}
