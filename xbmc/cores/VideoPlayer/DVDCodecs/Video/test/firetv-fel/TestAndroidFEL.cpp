/*
 *  Copyright (C) 2026 Team Kodi
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AndroidFEL.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <dvfelplay.h>

#define CHECK(expression) \
  do \
  { \
    if (!(expression)) \
    { \
      std::cerr << __LINE__ << ": " << #expression << '\n'; \
      std::abort(); \
    } \
  } while (false)

struct dvfp
{
};
namespace
{
dvfp handle;
struct Fake
{
  int apiVersion{DVFP_API_VERSION};
  int available{1};
  dvfp_rpu_info info{1, 7, DVFP_EL_FEL, 1};
  int probeError{0};
  bool openError{false};
  int filterError{0};
  bool emptyOutput{false};
  std::vector<std::string> events;
  std::vector<int64_t> pts;
} fake;
uint8_t input[]{1, 2, 3, 4};
uint8_t baseLayer[]{5, 6};

CAndroidFEL::Result Prepare(CAndroidFEL& fel)
{
  const uint8_t* output{};
  size_t size{};
  auto result = fel.Prepare(input, sizeof(input), output, size);
  if (result == CAndroidFEL::Result::FILTERED)
  {
    CHECK(output == baseLayer);
    CHECK(size == sizeof(baseLayer));
  }
  return result;
}
} // namespace

extern "C"
{
  int dvfp_api_version()
  {
    return fake.apiVersion;
  }
  int dvfp_available()
  {
    return fake.available;
  }
  int dvfp_probe(const uint8_t*, size_t, dvfp_rpu_info* info)
  {
    *info = fake.info;
    return fake.probeError;
  }
  dvfp* dvfp_open(int width, int height, int mode)
  {
    CHECK(width == 3840 && height == 2160 && mode == DVFP_MODE_FEL);
    fake.events.emplace_back("open");
    return fake.openError ? nullptr : &handle;
  }
  void dvfp_close(dvfp* h)
  {
    CHECK(h == &handle);
    fake.events.emplace_back("close");
  }
  int dvfp_filter(dvfp* h, const uint8_t*, size_t, const uint8_t** output, size_t* size)
  {
    CHECK(h == &handle);
    fake.events.emplace_back("filter");
    *output = fake.emptyOutput ? nullptr : baseLayer;
    *size = fake.emptyOutput ? 0 : sizeof(baseLayer);
    return fake.filterError;
  }
  void dvfp_commit(dvfp* h, int64_t pts)
  {
    CHECK(h == &handle);
    fake.events.emplace_back("commit");
    fake.pts.push_back(pts);
  }
  void dvfp_drop(dvfp* h)
  {
    CHECK(h == &handle);
    fake.events.emplace_back("drop");
  }
  void dvfp_flush(dvfp* h)
  {
    CHECK(h == &handle);
    fake.events.emplace_back("flush");
  }
}

int main()
{
  using Result = CAndroidFEL::Result;
  fake = {};
  CHECK(CAndroidFEL::IsAvailable());
  fake.apiVersion++;
  CHECK(!CAndroidFEL::IsAvailable());
  fake.apiVersion = DVFP_API_VERSION;
  fake.available = 0;
  CHECK(!CAndroidFEL::IsAvailable());
  std::cout << "PASS: missing helper and incompatible API disable FEL\n";

  fake = {};
  {
    CAndroidFEL fel(3840, 2160);
    CHECK(Prepare(fel) == Result::FILTERED);
    CHECK(fel.IsActive());
    CHECK(fake.pts.empty());
    fel.Commit(41708);
    fel.Commit(99999);
    CHECK(fake.pts == std::vector<int64_t>{41708});
  }
  CHECK(fake.events == (std::vector<std::string>{"open", "filter", "commit", "close"}));
  std::cout << "PASS: one EL commit per successful BL queue, exact MediaCodec timestamp\n";

  for (int scenario = 0; scenario < 4; ++scenario)
  {
    fake = {};
    if (scenario == 0)
      fake.info.el_type = DVFP_EL_MEL;
    if (scenario == 1)
      fake.info.profile = 8;
    if (scenario == 2)
      fake.info.has_el = 0;
    if (scenario == 3)
      fake.probeError = -1;
    CAndroidFEL fel(3840, 2160);
    CHECK(Prepare(fel) == Result::FALLBACK);
    CHECK(!fel.IsActive());
    CHECK(fake.events.empty());
  }
  std::cout << "PASS: MEL, P8, absent EL, and failed probe stay out of the composer\n";

  fake = {};
  {
    CAndroidFEL fel(3840, 2160);
    fake.info = {};
    CHECK(Prepare(fel) == Result::UNFILTERED);
    CHECK(fake.events.empty());
    fake.info = {1, 7, DVFP_EL_FEL, 1};
    CHECK(Prepare(fel) == Result::FILTERED);
    fel.Commit(0);
  }
  std::cout << "PASS: codec configuration before the first RPU does not disable FEL\n";

  fake = {};
  fake.openError = true;
  {
    CAndroidFEL fel(3840, 2160);
    CHECK(Prepare(fel) == Result::FALLBACK);
    CHECK(!fel.IsActive());
    fel.Commit(0);
  }
  CHECK(fake.events == std::vector<std::string>{"open"});
  std::cout << "PASS: EL decoder startup failure falls back without committing\n";

  for (int scenario = 0; scenario < 2; ++scenario)
  {
    fake = {};
    fake.filterError = scenario == 0 ? -1 : 0;
    fake.emptyOutput = scenario == 1;
    {
      CAndroidFEL fel(3840, 2160);
      CHECK(Prepare(fel) == Result::ERROR);
      fel.Commit(0);
    }
    CHECK(fake.events == (std::vector<std::string>{"open", "filter", "drop", "close"}));
  }
  std::cout << "PASS: failed/empty filter output cannot commit an EL frame\n";

  fake = {};
  {
    CAndroidFEL fel(3840, 2160);
    CHECK(Prepare(fel) == Result::FILTERED);
    fel.Drop(); // BL buffer too small or queueInputBuffer failed.
    fel.Commit(10);
    CHECK(fake.pts.empty());
    CHECK(Prepare(fel) == Result::FILTERED);
    fel.Commit(20);
  }
  CHECK(fake.pts == std::vector<int64_t>{20});
  std::cout << "PASS: rejected BL queue drops its EL; retry commits only the new packet\n";

  fake = {};
  {
    CAndroidFEL fel(3840, 2160);
    CHECK(Prepare(fel) == Result::FILTERED);
    fel.Commit(900000);
    CHECK(Prepare(fel) == Result::FILTERED);
    fel.Flush();
    fel.Commit(940000);
    CHECK(Prepare(fel) == Result::FILTERED);
    fel.Commit(0); // Kodi resets the timestamp origin on seek.
  }
  CHECK(fake.pts == (std::vector<int64_t>{900000, 0}));
  CHECK(fake.events == (std::vector<std::string>{"open", "filter", "commit", "filter", "drop",
                                                 "flush", "filter", "commit", "close"}));
  std::cout << "PASS: seek drops old pending EL and accepts the new timestamp origin\n";

  fake = {};
  {
    CAndroidFEL fel(3840, 2160);
    CHECK(Prepare(fel) == Result::FILTERED);
    CHECK(Prepare(fel) == Result::FILTERED);
  }
  CHECK(fake.events ==
        (std::vector<std::string>{"open", "filter", "drop", "filter", "drop", "close"}));
  std::cout << "PASS: superseded packets and shutdown discard pending EL before closing\n";
}
