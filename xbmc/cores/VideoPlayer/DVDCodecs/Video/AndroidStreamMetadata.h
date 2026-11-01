/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

// Diagnostic source metadata only. Never changes decoder input or output modes.
// RPU samples follow decoder input, not the exact frame currently on screen.
class CAndroidStreamMetadata
{
public:
  void Observe(const uint8_t* nal, size_t size);
  bool TakeChanges();
  const std::map<std::string, std::string>& Values() const { return m_values; }
private:
  void ReadSei(const uint8_t* nal, size_t size);
  void ReadRpu(const uint8_t* nal, size_t size);
  std::map<std::string, std::string> m_values;
  std::chrono::steady_clock::time_point m_nextRpu{};
  bool m_changed{false};
};
