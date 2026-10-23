/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "platform/OSXAutoTypes.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::osx {

struct InputSourceRecord
{
  std::string m_id;
  std::string m_language;
  bool m_hasUnicodeLayoutData = false;
  bool m_selectCapable = false;
  bool m_asciiCapable = false;
};

struct InputSourceChoice
{
  std::size_t m_index;
  std::string m_language;
  bool m_fallback;
};

struct TranslationInputSource
{
  AutoTISInputSourceRef m_source{nullptr, CFRelease};
  std::string m_language;
  bool m_fallback = false;
};

// layouts[0] is the current keyboard layout, layouts[1] the current ASCII
// layout, and subsequent entries are enabled layouts. This policy is pure:
// it neither queries TIS nor changes the user's selected input source.
std::optional<InputSourceChoice>
chooseTranslationInputSource(const InputSourceRecord &active, const std::vector<InputSourceRecord> &layouts);

// Accepts missing/malformed language properties without calling TIS.
std::string inputSourceLanguage(CFArrayRef languages);
std::string currentInputSourceLanguage();
TranslationInputSource resolveTranslationInputSource();

} // namespace deskflow::osx
