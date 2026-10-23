/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/unix/OSXInputSource.h"

#include <utility>

namespace deskflow::osx {
namespace {

constexpr auto kABCLayoutID = "com.apple.keylayout.ABC";
constexpr auto kUSLayoutID = "com.apple.keylayout.US";

std::string stringValue(CFStringRef value)
{
  if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
    return {};
  }
  const auto size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1;
  std::string result(size, '\0');
  if (!CFStringGetCString(value, result.data(), size, kCFStringEncodingUTF8)) {
    return {};
  }
  result.resize(std::char_traits<char>::length(result.c_str()));
  return result;
}

bool booleanValue(CFBooleanRef value)
{
  return value && CFGetTypeID(value) == CFBooleanGetTypeID() && CFBooleanGetValue(value);
}

// All callers hold g_tisMutex, including property reads.
InputSourceRecord describeSource(TISInputSourceRef source)
{
  if (!source) {
    return {};
  }
  InputSourceRecord result;
  result.m_id = stringValue((CFStringRef)TISGetInputSourceProperty(source, kTISPropertyInputSourceID));
  result.m_language =
      inputSourceLanguage((CFArrayRef)TISGetInputSourceProperty(source, kTISPropertyInputSourceLanguages));
  auto data = (CFDataRef)TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData);
  result.m_hasUnicodeLayoutData = data && CFGetTypeID(data) == CFDataGetTypeID() && CFDataGetLength(data) > 0;
  result.m_selectCapable =
      booleanValue((CFBooleanRef)TISGetInputSourceProperty(source, kTISPropertyInputSourceIsSelectCapable));
  result.m_asciiCapable =
      booleanValue((CFBooleanRef)TISGetInputSourceProperty(source, kTISPropertyInputSourceIsASCIICapable));
  return result;
}

std::string translationLanguage(const InputSourceRecord &source)
{
  if (!source.m_language.empty()) {
    return source.m_language;
  }
  // These built-in layouts have a known language even if TIS omits it.
  return source.m_id == kABCLayoutID || source.m_id == kUSLayoutID ? "en" : "";
}

} // namespace

std::string inputSourceLanguage(CFArrayRef languages)
{
  if (!languages || CFGetTypeID(languages) != CFArrayGetTypeID()) {
    return {};
  }
  for (CFIndex i = 0; i < CFArrayGetCount(languages); ++i) {
    auto language = stringValue((CFStringRef)CFArrayGetValueAtIndex(languages, i));
    if (language.size() == 2 || (language.size() > 2 && (language[2] == '-' || language[2] == '_'))) {
      return language.substr(0, 2);
    }
  }
  return {};
}

std::optional<InputSourceChoice>
chooseTranslationInputSource(const InputSourceRecord &active, const std::vector<InputSourceRecord> &layouts)
{
  if (layouts.empty()) {
    return std::nullopt;
  }
  const auto choice = [&layouts](std::size_t index, bool fallback) {
    return InputSourceChoice{index, translationLanguage(layouts[index]), fallback};
  };

  // Preserve upstream's handling of selectable layouts (including Dvorak).
  // An IME's private, non-selectable layout must not translate remote keys.
  if (layouts[0].m_hasUnicodeLayoutData && (active.m_hasUnicodeLayoutData || layouts[0].m_selectCapable)) {
    return choice(0, !active.m_hasUnicodeLayoutData);
  }
  // Preserve the user's current ASCII layout before trying ABC/US.
  if (layouts.size() > 1 && layouts[1].m_hasUnicodeLayoutData) {
    return choice(1, true);
  }
  for (const auto *id : {kABCLayoutID, kUSLayoutID}) {
    for (std::size_t i = 2; i < layouts.size(); ++i) {
      if (layouts[i].m_id == id && layouts[i].m_hasUnicodeLayoutData && layouts[i].m_selectCapable) {
        return choice(i, true);
      }
    }
  }
  for (std::size_t i = 2; i < layouts.size(); ++i) {
    if (layouts[i].m_hasUnicodeLayoutData && layouts[i].m_selectCapable && layouts[i].m_asciiCapable) {
      return choice(i, true);
    }
  }
  return std::nullopt;
}

std::string currentInputSourceLanguage()
{
  std::lock_guard<std::mutex> lock(g_tisMutex);
  AutoTISInputSourceRef source(TISCopyCurrentKeyboardInputSource(), CFRelease);
  return source ? inputSourceLanguage(
                      (CFArrayRef)TISGetInputSourceProperty(source.get(), kTISPropertyInputSourceLanguages)
                  )
                : std::string{};
}

TranslationInputSource resolveTranslationInputSource()
{
  std::lock_guard<std::mutex> lock(g_tisMutex);
  AutoTISInputSourceRef active(TISCopyCurrentKeyboardInputSource(), CFRelease);
  const auto activeRecord = describeSource(active.get());

  std::vector<AutoTISInputSourceRef> sources;
  sources.emplace_back(TISCopyCurrentKeyboardLayoutInputSource(), CFRelease);
  sources.emplace_back(TISCopyCurrentASCIICapableKeyboardLayoutInputSource(), CFRelease);
  std::vector<InputSourceRecord> records{describeSource(sources[0].get()), describeSource(sources[1].get())};
  auto choice = chooseTranslationInputSource(activeRecord, records);
  if (!choice) {
    CFStringRef keys[] = {kTISPropertyInputSourceCategory};
    CFStringRef values[] = {kTISCategoryKeyboardInputSource};
    AutoCFDictionary filter(
        CFDictionaryCreate(nullptr, (const void **)keys, (const void **)values, 1, nullptr, nullptr), CFRelease
    );
    AutoCFArray enabled(TISCreateInputSourceList(filter.get(), false), CFRelease);
    for (CFIndex i = 0; enabled && i < CFArrayGetCount(enabled.get()); ++i) {
      auto source = (TISInputSourceRef)CFArrayGetValueAtIndex(enabled.get(), i);
      if (source) {
        CFRetain(source);
        sources.emplace_back(source, CFRelease);
        records.push_back(describeSource(source));
      }
    }
    choice = chooseTranslationInputSource(activeRecord, records);
  }

  TranslationInputSource result;
  if (choice) {
    result.m_source = std::move(sources[choice->m_index]);
    result.m_language = choice->m_language;
    result.m_fallback = choice->m_fallback;
  }
  return result;
}

} // namespace deskflow::osx
