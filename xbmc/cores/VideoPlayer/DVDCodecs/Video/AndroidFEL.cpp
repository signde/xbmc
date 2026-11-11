/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AndroidFEL.h"

#include "ServiceBroker.h"
#include "cores/DataCacheCore.h"

#include <dvfelplay.h>

CAndroidFEL::~CAndroidFEL()
{
  Drop();
  if (m_handle)
    dvfp_close(m_handle);
}

bool CAndroidFEL::IsAvailable()
{
  return dvfp_api_version() == DVFP_API_VERSION && dvfp_available() == 1;
}

CAndroidFEL::Result CAndroidFEL::Prepare(const uint8_t* input,
                                         size_t size,
                                         const uint8_t*& output,
                                         size_t& outputSize)
{
  Drop();
  output = input;
  outputSize = size;
  if (!input || !size)
    return Result::ERROR;

  if (!m_handle)
  {
    dvfp_rpu_info info{};
    if (dvfp_probe(input, size, &info) != 0)
      return Result::FALLBACK;
    if (!info.has_rpu)
      return info.has_el ? Result::FALLBACK : Result::UNFILTERED;
    if (info.profile != 7 || info.el_type != DVFP_EL_FEL || !info.has_el)
    {
      CServiceBroker::GetDataCacheCore().SetVideoFELStatus(
          info.el_type == DVFP_EL_MEL ? "MEL base layer" : "Base layer fallback");
      return Result::FALLBACK;
    }

    m_handle = dvfp_open(m_width, m_height, DVFP_MODE_FEL);
    if (!m_handle)
    {
      CServiceBroker::GetDataCacheCore().SetVideoFELStatus("FEL unavailable");
      return Result::FALLBACK;
    }
    CServiceBroker::GetDataCacheCore().SetVideoFELStatus("FEL decoder active");
  }

  if (dvfp_filter(m_handle, input, size, &output, &outputSize) != 0 || !output || !outputSize)
  {
    dvfp_drop(m_handle);
    CServiceBroker::GetDataCacheCore().SetVideoFELStatus("FEL error");
    return Result::ERROR;
  }
  m_pending = true;
  return Result::FILTERED;
}

void CAndroidFEL::Commit(int64_t mediaCodecPtsUs)
{
  if (m_pending)
  {
    // EL must use the exact timestamp queued to the base-layer decoder.
    dvfp_commit(m_handle, mediaCodecPtsUs);
    m_pending = false;
  }
}

void CAndroidFEL::Drop()
{
  if (m_pending)
  {
    dvfp_drop(m_handle);
    m_pending = false;
  }
}

void CAndroidFEL::Flush()
{
  Drop();
  if (m_handle)
    dvfp_flush(m_handle);
}
