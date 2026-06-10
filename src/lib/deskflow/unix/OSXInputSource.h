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
  std::string m_type;
  std::string m_language;
  bool m_hasUnicodeLayoutData = false;
  bool m_asciiCapable = false;
};

struct InputSourceChoice
{
  std::size_t m_index = 0;
  bool m_fallback = false;
  std::string m_language;
};

struct TranslationInputSource
{
  AutoTISInputSourceRef m_source{nullptr, CFRelease};
  std::string m_activeID;
  std::string m_activeType;
  std::string m_activeLanguage;
  std::string m_selectedID;
  std::string m_selectedType;
  std::string m_language;
  bool m_fallback = false;
};

std::optional<InputSourceChoice>
chooseTranslationInputSource(const std::vector<InputSourceRecord> &records, std::size_t activeIndex);

TranslationInputSource resolveTranslationInputSource();
std::vector<std::string> getKeyboardLayoutLanguages();

} // namespace deskflow::osx
