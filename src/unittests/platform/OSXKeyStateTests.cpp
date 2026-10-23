/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2011 Nick Bolton
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXKeyStateTests.h"

#include "base/EventQueue.h"
#include "deskflow/unix/OSXInputSource.h"

#include <IOKit/hidsystem/IOLLEvent.h>
#include <array>
#include <memory>

using SourceRecord = deskflow::osx::InputSourceRecord;
using SourceRecords = std::vector<SourceRecord>;
Q_DECLARE_METATYPE(SourceRecord)
Q_DECLARE_METATYPE(SourceRecords)

namespace {

struct ModifierPair
{
  const char *m_name;
  uint32_t m_leftVK;
  uint32_t m_rightVK;
  KeyID m_leftKey;
  KeyID m_rightKey;
  KeyModifierMask m_mask;
  CGEventFlags m_flag;
  CGEventFlags m_leftFlag;
  CGEventFlags m_rightFlag;
};

const ModifierPair s_modifiers[] = {
    {"shift", kVK_Shift, kVK_RightShift, kKeyShift_L, kKeyShift_R, KeyModifierShift, kCGEventFlagMaskShift,
     NX_DEVICELSHIFTKEYMASK, NX_DEVICERSHIFTKEYMASK},
    {"control", kVK_Control, kVK_RightControl, kKeyControl_L, kKeyControl_R, KeyModifierControl,
     kCGEventFlagMaskControl, NX_DEVICELCTLKEYMASK, NX_DEVICERCTLKEYMASK},
    {"option", kVK_Option, kVK_RightOption, kKeyAlt_L, kKeyAlt_R, KeyModifierAlt, kCGEventFlagMaskAlternate,
     NX_DEVICELALTKEYMASK, NX_DEVICERALTKEYMASK},
    {"command", kVK_Command, kVK_RightCommand, kKeySuper_L, kKeySuper_R, KeyModifierSuper, kCGEventFlagMaskCommand,
     NX_DEVICELCMDKEYMASK, NX_DEVICERCMDKEYMASK}
};

void modifierTestData()
{
  QTest::addColumn<int>("modifierIndex");
  QTest::addColumn<bool>("rightFirst");
  QTest::addColumn<bool>("releaseFirst");
  for (int i = 0; i < 4; ++i) {
    for (const bool rightFirst : {false, true}) {
      for (const bool releaseFirst : {false, true}) {
        const auto name = QString("%1-%2-%3")
                              .arg(s_modifiers[i].m_name)
                              .arg(rightFirst ? "right-first" : "left-first")
                              .arg(releaseFirst ? "release-first" : "release-second");
        QTest::newRow(qPrintable(name)) << i << rightFirst << releaseFirst;
      }
    }
  }
}

struct SentKeyEvent
{
  bool m_press;
  KeyID m_key;
  KeyModifierMask m_mask;
  KeyButton m_button;
};

class StateOnlyOSXKeyState : public OSXKeyState
{
public:
  using OSXKeyState::getKeyboardEventFlags;
  using OSXKeyState::OSXKeyState;
  using OSXKeyState::setKeyboardModifiers;

  void sendKeyEvent(void *, bool press, bool, KeyID key, KeyModifierMask mask, int32_t, KeyButton button) override
  {
    m_sentKeyEvents.push_back({press, key, mask, button});
  }

  std::vector<SentKeyEvent> m_sentKeyEvents;

protected:
  // Guard against accidentally injecting real input from a unit test.
  void fakeKey(const Keystroke &) override
  {
    QFAIL("input injection is not allowed in state-only tests");
  }
};

class RecordingEventQueue : public EventQueue
{
public:
  void addEvent(Event &&event) override
  {
    std::unique_ptr<IKeyState::KeyInfo> info(static_cast<IKeyState::KeyInfo *>(event.getData()));
    m_types.push_back(event.getType());
    m_infos.push_back(*info);
  }

  std::vector<EventTypes> m_types;
  std::vector<IKeyState::KeyInfo> m_infos;
};

SourceRecord layout(std::string id, std::string language, bool selectable = true, bool ascii = true)
{
  return {std::move(id), std::move(language), true, selectable, ascii};
}

} // namespace

void OSXKeyStateTests::initTestCase()
{
  m_arch.init();
  m_log.setFilter(LogLevel::Level::Verbose);
}

void OSXKeyStateTests::mapModifiersFromOSX_OSXMask()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  StateOnlyOSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  for (const auto &modifier : s_modifiers) {
    QCOMPARE(keyState.mapModifiersFromOSX(modifier.m_flag), modifier.m_mask);
  }
  QCOMPARE(keyState.mapModifiersFromOSX(kCGEventFlagMaskAlphaShift), KeyModifierCapsLock);
  QCOMPARE(keyState.mapModifiersFromOSX(kCGEventFlagMaskNumericPad), KeyModifierNumLock);
}

void OSXKeyStateTests::mapKeyFromEvent_sidedModifiers()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  StateOnlyOSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  for (const auto &modifier : s_modifiers) {
    for (const bool right : {false, true}) {
      const auto vk = right ? modifier.m_rightVK : modifier.m_leftVK;
      const auto id = right ? modifier.m_rightKey : modifier.m_leftKey;
      std::unique_ptr<__CGEvent, decltype(&CFRelease)> event(CGEventCreateKeyboardEvent(nullptr, vk, true), CFRelease);
      QVERIFY(event);
      CGEventSetType(event.get(), kCGEventFlagsChanged);
      OSXKeyState::KeyIDs ids;
      std::string language = "stale";
      QCOMPARE(keyState.mapKeyFromEvent(ids, nullptr, event.get(), &language), static_cast<KeyButton>(vk + 1));
      QCOMPARE(ids.size(), std::size_t{1});
      QCOMPARE(ids.front(), id);
      QVERIFY(language.empty());
      CGEventSetType(event.get(), kCGEventKeyUp);
      language = "stale";
      QCOMPARE(keyState.mapKeyFromEvent(ids, nullptr, event.get(), &language), static_cast<KeyButton>(vk + 1));
      QCOMPARE(ids.front(), kKeyNone);
      QVERIFY(language.empty());
    }
  }
}

void OSXKeyStateTests::handleModifierKeys_sidedModifiers_data()
{
  modifierTestData();
}

void OSXKeyStateTests::handleModifierKeys_sidedModifiers()
{
  QFETCH(int, modifierIndex);
  QFETCH(bool, rightFirst);
  QFETCH(bool, releaseFirst);
  const auto &modifier = s_modifiers[modifierIndex];
  const std::array<uint32_t, 2> vk = rightFirst ? std::array{modifier.m_rightVK, modifier.m_leftVK}
                                                : std::array{modifier.m_leftVK, modifier.m_rightVK};
  const std::array<KeyID, 2> id = rightFirst ? std::array{modifier.m_rightKey, modifier.m_leftKey}
                                             : std::array{modifier.m_leftKey, modifier.m_rightKey};
  const std::array<CGEventFlags, 2> flags = rightFirst ? std::array{modifier.m_rightFlag, modifier.m_leftFlag}
                                                       : std::array{modifier.m_leftFlag, modifier.m_rightFlag};
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  StateOnlyOSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  const auto handle = [&](uint32_t key, CGEventFlags deviceFlags, KeyModifierMask mask) {
    keyState.handleModifierKeys(this, key, modifier.m_flag | deviceFlags, keyState.getActiveModifiers(), mask);
  };
  // Check an isolated press/release before the overlapping sequence.
  handle(vk[0], flags[0], modifier.m_mask);
  keyState.handleModifierKeys(this, vk[0], 0, modifier.m_mask, 0);
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{2});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, id[0]);
  QVERIFY(!keyState.m_sentKeyEvents.back().m_press);
  QVERIFY(!keyState.isKeyDown(vk[0] + 1));
  QCOMPARE(keyState.getActiveModifiers(), KeyModifierMask{0});
  keyState.m_sentKeyEvents.clear();

  handle(vk[0], flags[0], modifier.m_mask);
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{1});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, id[0]);
  QVERIFY(keyState.m_sentKeyEvents.back().m_press);
  // A duplicate flagsChanged must not be sent as another key down.
  handle(vk[0], flags[0], modifier.m_mask);
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{1});
  handle(vk[1], flags[0] | flags[1], modifier.m_mask);
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{2});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, id[1]);
  QVERIFY(keyState.isKeyDown(vk[0] + 1));
  QVERIFY(keyState.isKeyDown(vk[1] + 1));

  const auto released = releaseFirst ? 0 : 1;
  const auto held = 1 - released;
  handle(vk[released], flags[held], modifier.m_mask);
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{3});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, id[released]);
  QVERIFY(!keyState.m_sentKeyEvents.back().m_press);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_button, static_cast<KeyButton>(vk[released] + 1));
  QCOMPARE(keyState.m_sentKeyEvents.back().m_mask, modifier.m_mask);
  QVERIFY(!keyState.isKeyDown(vk[released] + 1));
  QVERIFY(keyState.isKeyDown(vk[held] + 1));
  QCOMPARE(keyState.getActiveModifiers(), modifier.m_mask);

  keyState.handleModifierKeys(this, vk[held], 0, modifier.m_mask, 0);
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{4});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, id[held]);
  QVERIFY(!keyState.m_sentKeyEvents.back().m_press);
  QVERIFY(!keyState.isKeyDown(vk[held] + 1));
  QCOMPARE(keyState.getActiveModifiers(), KeyModifierMask{0});
}

void OSXKeyStateTests::clientModifierState_data()
{
  modifierTestData();
}

void OSXKeyStateTests::clientModifierState()
{
  QFETCH(int, modifierIndex);
  QFETCH(bool, rightFirst);
  QFETCH(bool, releaseFirst);
  const auto &modifier = s_modifiers[modifierIndex];
  const std::array<uint32_t, 2> vk = rightFirst ? std::array{modifier.m_rightVK, modifier.m_leftVK}
                                                : std::array{modifier.m_leftVK, modifier.m_rightVK};
  const std::array<CGEventFlags, 2> flags = rightFirst ? std::array{modifier.m_rightFlag, modifier.m_leftFlag}
                                                       : std::array{modifier.m_leftFlag, modifier.m_rightFlag};
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  StateOnlyOSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  keyState.setKeyboardModifiers(vk[0], true);
  QCOMPARE(keyState.getKeyboardEventFlags(), modifier.m_flag | flags[0]);
  keyState.setKeyboardModifiers(vk[0], false);
  QCOMPARE(keyState.getKeyboardEventFlags(), CGEventFlags{0});
  keyState.setKeyboardModifiers(vk[0], true);
  keyState.setKeyboardModifiers(vk[1], true);
  QCOMPARE(keyState.getKeyboardEventFlags(), modifier.m_flag | flags[0] | flags[1]);
  keyState.setKeyboardModifiers(vk[releaseFirst ? 0 : 1], false);
  QCOMPARE(keyState.getKeyboardEventFlags(), modifier.m_flag | flags[releaseFirst ? 1 : 0]);
  QCOMPARE(keyState.getModifierStateAsOSXFlags(), modifier.m_flag);
  keyState.setKeyboardModifiers(vk[releaseFirst ? 1 : 0], false);
  QCOMPARE(keyState.getKeyboardEventFlags(), CGEventFlags{0});
}

void OSXKeyStateTests::capsLockAndNonModifierState()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  StateOnlyOSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  keyState.setKeyboardModifiers(kVK_CapsLock, true);
  keyState.setKeyboardModifiers(kVK_RightControl, true);
  const CGEventFlags expected = kCGEventFlagMaskAlphaShift | kCGEventFlagMaskControl | NX_DEVICERCTLKEYMASK;
  QCOMPARE(keyState.getKeyboardEventFlags(), expected);
  keyState.setKeyboardModifiers(kVK_ANSI_A, true);
  keyState.setKeyboardModifiers(kVK_ANSI_A, false);
  QCOMPARE(keyState.getKeyboardEventFlags(), expected);
  keyState.setKeyboardModifiers(kVK_RightControl, false);
  QCOMPARE(keyState.getKeyboardEventFlags(), CGEventFlags{kCGEventFlagMaskAlphaShift});
  keyState.setKeyboardModifiers(kVK_CapsLock, false);
  QCOMPARE(keyState.getKeyboardEventFlags(), CGEventFlags{0});

  keyState.handleModifierKeys(this, kVK_CapsLock, kCGEventFlagMaskAlphaShift, 0, KeyModifierCapsLock);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, kKeyCapsLock);
  QVERIFY(keyState.m_sentKeyEvents.back().m_press);
  keyState.handleModifierKeys(
      this, kVK_Function, kCGEventFlagMaskAlphaShift | kCGEventFlagMaskSecondaryFn, KeyModifierCapsLock,
      KeyModifierCapsLock
  );
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{1});
  QCOMPARE(keyState.getActiveModifiers(), KeyModifierCapsLock);
  keyState.handleModifierKeys(this, kVK_CapsLock, 0, KeyModifierCapsLock, 0);
  QVERIFY(!keyState.m_sentKeyEvents.back().m_press);
  keyState.handleModifierKeys(this, kVK_ANSI_KeypadClear, kCGEventFlagMaskNumericPad, 0, KeyModifierNumLock);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, kKeyNumLock);
  QVERIFY(keyState.m_sentKeyEvents.back().m_press);
}

void OSXKeyStateTests::chooseTranslationInputSource_data()
{
  QTest::addColumn<SourceRecord>("active");
  QTest::addColumn<SourceRecords>("layouts");
  QTest::addColumn<int>("expectedIndex");
  QTest::addColumn<QString>("language");
  QTest::addColumn<bool>("fallback");
  const SourceRecord ime{"com.apple.inputmethod.Korean.2SetKorean", "ko"};
  const auto abc = layout("com.apple.keylayout.ABC", "en");
  const auto us = layout("com.apple.keylayout.US", "en");
  const auto dvorak = layout("com.apple.keylayout.Dvorak", "en");
  const auto german = layout("com.apple.keylayout.German", "de");
  const auto privateIME = layout("com.example.inputmethod.private", "ko", false, false);

  QTest::newRow("active-ABC") << abc << SourceRecords{abc, us} << 0 << QString("en") << false;
  QTest::newRow("active-Dvorak") << dvorak << SourceRecords{dvorak, abc} << 0 << QString("en") << false;
  const auto russian = layout("com.apple.keylayout.Russian", "ru", true, false);
  QTest::newRow("active-non-ASCII-layout") << russian << SourceRecords{russian, abc} << 0 << QString("ru") << false;
  QTest::newRow("active-layout-data") << privateIME << SourceRecords{privateIME, abc} << 0 << QString("ko") << false;
  QTest::newRow("missing-current-layout") << ime << SourceRecords{{}, abc} << 1 << QString("en") << true;
  QTest::newRow("IME-selectable-layout") << ime << SourceRecords{dvorak, abc} << 0 << QString("en") << true;
  QTest::newRow("IME-private-to-current-ASCII")
      << ime << SourceRecords{privateIME, dvorak, abc} << 1 << QString("en") << true;
  QTest::newRow("IME-preserve-ASCII-language")
      << ime << SourceRecords{privateIME, german, abc} << 1 << QString("de") << true;
  QTest::newRow("IME-fallback-ABC-before-US")
      << ime << SourceRecords{privateIME, {}, us, abc} << 3 << QString("en") << true;
  QTest::newRow("IME-fallback-US") << ime << SourceRecords{privateIME, {}, us} << 2 << QString("en") << true;
  QTest::newRow("skip-non-selectable-ABC")
      << ime << SourceRecords{privateIME, {}, layout(abc.m_id, "en", false), us} << 3 << QString("en") << true;
  QTest::newRow("reject-non-ASCII-fallback")
      << ime << SourceRecords{privateIME, {}, russian} << -1 << QString() << false;
  QTest::newRow("IME-other-ASCII") << ime << SourceRecords{privateIME, {}, german} << 2 << QString("de") << true;
  QTest::newRow("ABC-missing-language") << ime << SourceRecords{privateIME, {}, layout("com.apple.keylayout.ABC", "")}
                                        << 2 << QString("en") << true;
  QTest::newRow("reject-private-fallback")
      << ime << SourceRecords{privateIME, {}, privateIME} << -1 << QString() << false;
  QTest::newRow("missing-properties") << SourceRecord{} << SourceRecords{{}, {}} << -1 << QString() << false;
  QTest::newRow("missing-layout-data") << ime << SourceRecords{{"com.apple.keylayout.ABC", "en"}, {}} << -1 << QString()
                                       << false;
  QTest::newRow("no-layouts") << ime << SourceRecords{} << -1 << QString() << false;
}

void OSXKeyStateTests::chooseTranslationInputSource()
{
  QFETCH(SourceRecord, active);
  QFETCH(SourceRecords, layouts);
  QFETCH(int, expectedIndex);
  QFETCH(QString, language);
  QFETCH(bool, fallback);
  const auto choice = deskflow::osx::chooseTranslationInputSource(active, layouts);
  QCOMPARE(choice.has_value(), expectedIndex >= 0);
  if (choice) {
    QCOMPARE(choice->m_index, static_cast<std::size_t>(expectedIndex));
    QCOMPARE(QString::fromStdString(choice->m_language), language);
    QCOMPARE(choice->m_fallback, fallback);
  }
}

void OSXKeyStateTests::inputSourceLanguage_missingProperties()
{
  using deskflow::osx::inputSourceLanguage;
  QVERIFY(inputSourceLanguage(nullptr).empty());
  QVERIFY(inputSourceLanguage((CFArrayRef)kCFBooleanTrue).empty());
  AutoCFArray empty(CFArrayCreate(nullptr, nullptr, 0, &kCFTypeArrayCallBacks), CFRelease);
  QVERIFY(inputSourceLanguage(empty.get()).empty());
  const void *values[] = {kCFNull, CFSTR(""), CFSTR("invalid"), CFSTR("ko-KR"), CFSTR("en")};
  AutoCFArray languages(CFArrayCreate(nullptr, values, 5, &kCFTypeArrayCallBacks), CFRelease);
  QCOMPARE(inputSourceLanguage(languages.get()), std::string("ko"));
}

void OSXKeyStateTests::keyInfo_translationLanguage()
{
  using KeyInfo = IKeyState::KeyInfo;
  std::unique_ptr<KeyInfo> info(KeyInfo::allocWithLanguage('a', KeyModifierShift, 1, 2, "en"));
  info->m_computers = ":remote:";
  std::unique_ptr<KeyInfo> copy(KeyInfo::alloc(*info));
  QVERIFY(KeyInfo::equal(info.get(), copy.get()));
  QCOMPARE(copy->m_language, std::string("en"));
  QCOMPARE(copy->m_computers, std::string(":remote:"));
  copy->m_language = "ko";
  QVERIFY(!KeyInfo::equal(info.get(), copy.get()));
  std::unique_ptr<KeyInfo> legacy(KeyInfo::alloc('a', 0, 1, 1));
  QVERIFY(legacy->m_language.empty());
  std::unique_ptr<KeyInfo> destinations(KeyInfo::alloc('a', 0, 1, 1, {"remote"}));
  QVERIFY(destinations->m_language.empty());
}

void OSXKeyStateTests::sendKeyEvent_translationLanguage()
{
  deskflow::KeyMap keyMap;
  RecordingEventQueue eventQueue;
  OSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  keyState.sendKeyEvent(this, true, false, 'a', 0, 1, 1, "en");
  keyState.sendKeyEvent(this, true, true, 'a', 0, 3, 1, "en");
  keyState.sendKeyEvent(this, false, false, 'a', 0, 1, 1, "en");
  QCOMPARE(
      eventQueue.m_types,
      (std::vector{EventTypes::KeyStateKeyDown, EventTypes::KeyStateKeyRepeat, EventTypes::KeyStateKeyUp})
  );
  QCOMPARE(eventQueue.m_infos[1].m_count, 3);
  for (const auto &info : eventQueue.m_infos) {
    QCOMPARE(info.m_language, std::string("en"));
  }
  keyMap.addHalfDuplexButton(kVK_CapsLock + 1);
  keyState.sendKeyEvent(this, true, false, kKeyCapsLock, KeyModifierCapsLock, 1, kVK_CapsLock + 1, "en");
  QCOMPARE(eventQueue.m_infos.size(), std::size_t{5});
  QCOMPARE(eventQueue.m_infos[3].m_language, std::string("en"));
  QCOMPARE(eventQueue.m_infos[4].m_language, std::string("en"));
  keyState.sendKeyEvent(this, true, false, 'b', 0, 1, 2);
  QVERIFY(eventQueue.m_infos.back().m_language.empty());
}

QTEST_GUILESS_MAIN(OSXKeyStateTests)
