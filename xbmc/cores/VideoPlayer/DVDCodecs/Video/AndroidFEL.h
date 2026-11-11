/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstddef>
#include <cstdint>

struct dvfp;

// Owns one libdvfelplay session. Use only from the MediaCodec input thread.
class CAndroidFEL
{
public:
  enum class Result
  {
    UNFILTERED,
    FILTERED,
    FALLBACK,
    ERROR
  };

  CAndroidFEL(int width, int height) : m_width(width), m_height(height) {}
  ~CAndroidFEL();
  CAndroidFEL(const CAndroidFEL&) = delete;
  CAndroidFEL& operator=(const CAndroidFEL&) = delete;

  static bool IsAvailable();
  Result Prepare(const uint8_t* input, size_t size, const uint8_t*& output, size_t& outputSize);
  void Commit(int64_t mediaCodecPtsUs);
  void Drop();
  void Flush();
  bool IsActive() const { return m_handle != nullptr; }

private:
  dvfp* m_handle{nullptr};
  int m_width;
  int m_height;
  bool m_pending{false};
};
