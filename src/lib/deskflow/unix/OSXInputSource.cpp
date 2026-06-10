/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/unix/OSXInputSource.h"

#include <Carbon/Carbon.h>

#include <algorithm>
#include <array>
#include <utility>

namespace deskflow::osx {
namespace {

constexpr auto kAppleABCLayoutID = "com.apple.keylayout.ABC";
constexpr auto kAppleUSLayoutID = "com.apple.keylayout.US";

struct SourceInfo
{
  AutoTISInputSourceRef m_source{nullptr, CFRelease};
  InputSourceRecord m_record;
};

using SourceList = std::vector<SourceInfo>;

std::string cfStringToString(CFStringRef value)
{
  if (value == nullptr) {
    return {};
  }

  CFIndex length = CFStringGetLength(value);
  CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::string result(static_cast<std::size_t>(maxSize), '\0');
  if (!CFStringGetCString(value, result.data(), maxSize, kCFStringEncodingUTF8)) {
    return {};
  }

  result.resize(std::char_traits<char>::length(result.c_str()));
  return result;
}

std::string sourceStringProperty(TISInputSourceRef source, CFStringRef property)
{
  return cfStringToString(static_cast<CFStringRef>(TISGetInputSourceProperty(source, property)));
}

std::string sourceLanguage(TISInputSourceRef source)
{
  auto languages = static_cast<CFArrayRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceLanguages));
  if (languages == nullptr) {
    return {};
  }

  for (CFIndex i = 0; i < CFArrayGetCount(languages); ++i) {
    auto language = static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages, i));
    std::string result = cfStringToString(language);
    if (!result.empty()) {
      return result;
    }
  }

  return {};
}

bool sourceIsKeyboardLayout(TISInputSourceRef source)
{
  return sourceStringProperty(source, kTISPropertyInputSourceType) == cfStringToString(kTISTypeKeyboardLayout);
}

CFDataRef sourceUnicodeLayoutData(TISInputSourceRef source)
{
  return static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
}

bool sourceHasUnicodeLayoutData(TISInputSourceRef source)
{
  auto data = sourceUnicodeLayoutData(source);
  return data != nullptr && CFDataGetLength(data) > 0;
}

bool sourceIsAsciiCapable(TISInputSourceRef source)
{
  auto data = sourceUnicodeLayoutData(source);
  if (data == nullptr || CFDataGetLength(data) == 0) {
    return false;
  }

  auto layout = reinterpret_cast<const UCKeyboardLayout *>(CFDataGetBytePtr(data));
  if (layout == nullptr) {
    return false;
  }

  uint32_t deadKeyState = 0;
  UniCharCount count = 0;
  std::array<UniChar, 4> chars{};
  OSStatus status = UCKeyTranslate(
      layout, kVK_ANSI_A, kUCKeyActionDown, 0, LMGetKbdType(), kUCKeyTranslateNoDeadKeysBit, &deadKeyState,
      chars.size(), &count, chars.data()
  );

  return status == 0 && count > 0 && chars[0] < 0x80;
}

InputSourceRecord makeRecord(TISInputSourceRef source)
{
  InputSourceRecord record;
  record.m_id = sourceStringProperty(source, kTISPropertyInputSourceID);
  record.m_type = sourceStringProperty(source, kTISPropertyInputSourceType);
  record.m_language = sourceLanguage(source);
  record.m_hasUnicodeLayoutData = sourceHasUnicodeLayoutData(source);
  record.m_asciiCapable = sourceIsKeyboardLayout(source) && record.m_hasUnicodeLayoutData && sourceIsAsciiCapable(source);
  return record;
}

AutoTISInputSourceRef retainSource(TISInputSourceRef source)
{
  if (source != nullptr) {
    CFRetain(source);
  }
  return AutoTISInputSourceRef(source, CFRelease);
}

std::string twoLetterLanguage(const std::string &language)
{
  if (language.size() >= 2) {
    return language.substr(0, 2);
  }
  return language;
}

std::optional<std::size_t> findSourceIndex(const SourceList &sources, TISInputSourceRef source)
{
  if (source == nullptr) {
    return std::nullopt;
  }

  const std::string id = sourceStringProperty(source, kTISPropertyInputSourceID);
  if (!id.empty()) {
    for (std::size_t i = 0; i < sources.size(); ++i) {
      if (sources[i].m_record.m_id == id) {
        return i;
      }
    }
  }

  return std::nullopt;
}

SourceList getEnabledKeyboardSources()
{
  SourceList sources;

  CFStringRef keys[] = {kTISPropertyInputSourceCategory};
  CFStringRef values[] = {kTISCategoryKeyboardInputSource};
  AutoCFDictionary dict(
      CFDictionaryCreate(nullptr, reinterpret_cast<const void **>(keys), reinterpret_cast<const void **>(values), 1,
                         nullptr, nullptr),
      CFRelease
  );
  AutoCFArray inputSources(TISCreateInputSourceList(dict.get(), false), CFRelease);
  if (!inputSources) {
    return sources;
  }

  for (CFIndex i = 0; i < CFArrayGetCount(inputSources.get()); ++i) {
    auto source = const_cast<TISInputSourceRef>(
        static_cast<const __TISInputSource *>(CFArrayGetValueAtIndex(inputSources.get(), i))
    );
    if (source == nullptr) {
      continue;
    }

    SourceInfo info;
    info.m_source = retainSource(source);
    info.m_record = makeRecord(source);
    sources.push_back(std::move(info));
  }

  return sources;
}

InputSourceChoice makeChoice(std::size_t index, bool fallback, const std::vector<InputSourceRecord> &records)
{
  InputSourceChoice choice;
  choice.m_index = index;
  choice.m_fallback = fallback;
  choice.m_language = fallback ? "en" : twoLetterLanguage(records[index].m_language);
  return choice;
}

void fillSourceMetadata(TranslationInputSource &result, const SourceList &sources, const InputSourceChoice &choice)
{
  result.m_source = retainSource(sources[choice.m_index].m_source.get());
  result.m_selectedID = sources[choice.m_index].m_record.m_id;
  result.m_selectedType = sources[choice.m_index].m_record.m_type;
  result.m_language = choice.m_language;
  result.m_fallback = choice.m_fallback;
}

} // namespace

std::optional<InputSourceChoice>
chooseTranslationInputSource(const std::vector<InputSourceRecord> &records, std::size_t activeIndex)
{
  if (records.empty() || activeIndex >= records.size()) {
    return std::nullopt;
  }

  const auto &active = records[activeIndex];
  const bool activeIsUsableKeyboardLayout =
      active.m_type == cfStringToString(kTISTypeKeyboardLayout) && active.m_hasUnicodeLayoutData;
  if (activeIsUsableKeyboardLayout) {
    return makeChoice(activeIndex, false, records);
  }

  for (const auto &layoutID : {kAppleABCLayoutID, kAppleUSLayoutID}) {
    for (std::size_t i = 0; i < records.size(); ++i) {
      if (records[i].m_id == layoutID && records[i].m_hasUnicodeLayoutData) {
        return makeChoice(i, true, records);
      }
    }
  }

  for (std::size_t i = 0; i < records.size(); ++i) {
    if (records[i].m_asciiCapable && records[i].m_hasUnicodeLayoutData) {
      return makeChoice(i, true, records);
    }
  }

  if (active.m_hasUnicodeLayoutData) {
    return makeChoice(activeIndex, true, records);
  }

  return std::nullopt;
}

TranslationInputSource resolveTranslationInputSource()
{
  TranslationInputSource result;
  SourceList sources = getEnabledKeyboardSources();

  AutoTISInputSourceRef activeSource(TISCopyCurrentKeyboardInputSource(), CFRelease);
  if (activeSource) {
    InputSourceRecord activeRecord = makeRecord(activeSource.get());
    result.m_activeID = activeRecord.m_id;
    result.m_activeType = activeRecord.m_type;
    result.m_activeLanguage = twoLetterLanguage(activeRecord.m_language);
  }

  auto activeIndex = findSourceIndex(sources, activeSource.get());
  if (!activeIndex && activeSource) {
    InputSourceRecord activeRecord = makeRecord(activeSource.get());
    SourceInfo info;
    info.m_source = retainSource(activeSource.get());
    info.m_record = activeRecord;
    sources.push_back(std::move(info));
    activeIndex = sources.size() - 1;
  }

  if (activeIndex) {
    std::vector<InputSourceRecord> records;
    records.reserve(sources.size());
    for (const auto &source : sources) {
      records.push_back(source.m_record);
    }

    if (auto choice = chooseTranslationInputSource(records, *activeIndex)) {
      fillSourceMetadata(result, sources, *choice);
      return result;
    }
  }

  AutoTISInputSourceRef currentLayout(TISCopyCurrentKeyboardLayoutInputSource(), CFRelease);
  if (currentLayout && sourceHasUnicodeLayoutData(currentLayout.get())) {
    InputSourceRecord record = makeRecord(currentLayout.get());
    result.m_source = retainSource(currentLayout.get());
    result.m_selectedID = record.m_id;
    result.m_selectedType = record.m_type;
    result.m_language = twoLetterLanguage(record.m_language);
    result.m_fallback = true;
  }

  return result;
}

std::vector<std::string> getKeyboardLayoutLanguages()
{
  std::vector<std::string> languages;

  SourceList sources = getEnabledKeyboardSources();
  for (const auto &source : sources) {
    const auto &record = source.m_record;
    if (record.m_type != cfStringToString(kTISTypeKeyboardLayout) || !record.m_hasUnicodeLayoutData) {
      continue;
    }

    std::string language = twoLetterLanguage(record.m_language);
    if (language.size() == 2 && std::find(languages.begin(), languages.end(), language) == languages.end()) {
      languages.push_back(language);
    }
  }

  return languages;
}

} // namespace deskflow::osx
