/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "AndroidStreamMetadata.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>
#ifdef HAVE_LIBDOVI
#include <libdovi/rpu_parser.h>
#endif

namespace
{
std::string Decimal(double value, int precision)
{
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << value;
  return out.str();
}
uint32_t BE(const uint8_t* data, size_t bytes)
{
  uint32_t value = 0;
  for (size_t i = 0; i < bytes; ++i)
    value = (value << 8) | data[i];
  return value;
}
#ifdef HAVE_LIBDOVI
std::string Nits(uint16_t code)
{
  // ST 2084, normalized 12-bit Dolby Vision PQ codes.
  const double p = std::pow(std::min(code, uint16_t{4095}) / 4095.0, 1.0 / 78.84375);
  const double nits = 10000.0 * std::pow(std::max(p - 0.8359375, 0.0) /
                                           (18.8515625 - 18.6875 * p), 1.0 / 0.1593017578125);
  return Decimal(nits, nits < 1.0 ? 4 : 1);
}
#endif
}

bool CAndroidStreamMetadata::TakeChanges()
{
  const bool changed = m_changed;
  m_changed = false;
  return changed;
}

void CAndroidStreamMetadata::Observe(const uint8_t* nal, size_t size)
{
  if (!nal || size < 2)
    return;
  const unsigned type = (nal[0] >> 1) & 63;
  // Bound work on malformed/untrusted inputs. These metadata NALs are small.
  if (type == 39 || type == 40)
  {
    if (size <= 65536)
      ReadSei(nal, size);
  }
  else if (type == 62 && size <= 65536)
  {
    const auto now = std::chrono::steady_clock::now();
    if (now >= m_nextRpu)
    {
      m_nextRpu = now + std::chrono::milliseconds(250);
      ReadRpu(nal, size);
    }
  }
}

void CAndroidStreamMetadata::ReadSei(const uint8_t* nal, size_t size)
{
  // Only base-layer SEI, not the separately coded enhancement layer.
  if (((nal[0] & 1) << 5 | (nal[1] >> 3)) != 0)
    return;
  std::vector<uint8_t> rbsp;
  rbsp.reserve(size - 2);
  unsigned zeros = 0;
  for (size_t i = 2; i < size; ++i)
  {
    const uint8_t b = nal[i];
    if (zeros >= 2 && b == 3)
    {
      if (i + 1 >= size || nal[i + 1] > 3)
        return;
      zeros = 0;
      continue;
    }
    rbsp.push_back(b);
    zeros = b == 0 ? zeros + 1 : 0;
  }
  size_t pos = 0;
  while (pos < rbsp.size())
  {
    if (pos + 1 == rbsp.size() && rbsp[pos] == 0x80)
      break;
    size_t type = 0, length = 0;
    uint8_t b;
    do
    {
      if (pos >= rbsp.size()) return;
      b = rbsp[pos++];
      type += b;
    } while (b == 255);
    do
    {
      if (pos >= rbsp.size()) return;
      b = rbsp[pos++];
      length += b;
    } while (b == 255);
    if (length > rbsp.size() - pos)
      return;
    const auto* data = rbsp.data() + pos;
    if (type == 137 && length == 24)
    {
      m_values["video.hdr.max.lum"] = Decimal(BE(data + 16, 4) / 10000.0, 0);
      m_values["video.hdr.min.lum"] = Decimal(BE(data + 20, 4) / 10000.0, 4);
      m_changed = true;
    }
    else if (type == 144 && length == 4)
    {
      m_values["video.hdr.max.cll"] = std::to_string(BE(data, 2));
      m_values["video.hdr.max.fall"] = std::to_string(BE(data + 2, 2));
      m_changed = true;
    }
    pos += length;
  }
}

void CAndroidStreamMetadata::ReadRpu(const uint8_t* nal, size_t size)
{
#ifdef HAVE_LIBDOVI
  DoviRpuOpaque* rpu = dovi_parse_unspec62_nalu(nal, size);
  if (!rpu)
    return;
  const DoviVdrDmData* dm = dovi_rpu_get_vdr_dm_data(rpu);
  // Do not carry L1/L5/L6 from a previous sampled RPU if this one lacks them.
  for (auto it = m_values.begin(); it != m_values.end();)
  {
    if (it->first.rfind("video.dovi.", 0) == 0)
      it = m_values.erase(it);
    else
      ++it;
  }
  m_changed = true;
  if (dm && !dm->compressed)
  {
    const auto& blocks = dm->dm_data;
    m_values["video.dovi.meta.version"] = blocks.level254 ? "4.0" : "2.9";
    m_values["video.dovi.source.min.pq"] = std::to_string(dm->source_min_pq);
    m_values["video.dovi.source.max.pq"] = std::to_string(dm->source_max_pq);
    m_values["video.dovi.source.min.nits"] = Nits(dm->source_min_pq);
    m_values["video.dovi.source.max.nits"] = Nits(dm->source_max_pq);
    if (const auto* l1 = blocks.level1)
    {
      m_values["video.dovi.l1.min.pq"] = std::to_string(l1->min_pq);
      m_values["video.dovi.l1.max.pq"] = std::to_string(l1->max_pq);
      m_values["video.dovi.l1.avg.pq"] = std::to_string(l1->avg_pq);
      m_values["video.dovi.l1.min.nits"] = Nits(l1->min_pq);
      m_values["video.dovi.l1.max.nits"] = Nits(l1->max_pq);
      m_values["video.dovi.l1.avg.nits"] = Nits(l1->avg_pq);
    }
    if (const auto* l5 = blocks.level5)
    {
      m_values["video.dovi.l5.left.offset"] = std::to_string(l5->active_area_left_offset);
      m_values["video.dovi.l5.right.offset"] = std::to_string(l5->active_area_right_offset);
      m_values["video.dovi.l5.top.offset"] = std::to_string(l5->active_area_top_offset);
      m_values["video.dovi.l5.bottom.offset"] = std::to_string(l5->active_area_bottom_offset);
    }
    if (const auto* l6 = blocks.level6)
    {
      m_values["video.dovi.l6.min.lum"] = Decimal(l6->min_display_mastering_luminance / 10000.0, 4);
      m_values["video.dovi.l6.max.lum"] = std::to_string(l6->max_display_mastering_luminance);
      m_values["video.dovi.l6.max.cll"] = std::to_string(l6->max_content_light_level);
      m_values["video.dovi.l6.max.fall"] = std::to_string(l6->max_frame_average_light_level);
    }
  }
  if (dm)
    dovi_rpu_free_vdr_dm_data(dm);
  dovi_rpu_free(rpu);
#endif
}
