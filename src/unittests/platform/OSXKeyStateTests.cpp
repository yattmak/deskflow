/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2011 Nick Bolton
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXKeyStateTests.h"

#include "base/EventQueue.h"
#include "deskflow/unix/OSXInputSource.h"

#include <Carbon/Carbon.h>
#include <QString>
#include <vector>

#define SHIFT_ID_L kKeyShift_L
#define SHIFT_ID_R kKeyShift_R
#define SHIFT_BUTTON_L 57
#define SHIFT_BUTTON_R 61
#define A_CHAR_ID 0x00000061
#define A_CHAR_BUTTON 001

namespace {

struct SentKeyEvent
{
  bool m_press;
  KeyID m_key;
  KeyModifierMask m_mask;
  KeyButton m_button;
};

class RecordingOSXKeyState : public OSXKeyState
{
public:
  using OSXKeyState::OSXKeyState;

  void sendKeyEvent(
      void *, bool press, bool, KeyID key, KeyModifierMask mask, int32_t, KeyButton button
  ) override
  {
    m_sentKeyEvents.push_back({press, key, mask, button});
  }

  std::vector<SentKeyEvent> m_sentKeyEvents;
};

std::string cfStringToString(CFStringRef value)
{
  char buffer[128] = {0};
  if (value == nullptr || !CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
    return {};
  }
  return buffer;
}

deskflow::osx::InputSourceRecord makeInputSourceRecord(
    const std::string &id, const std::string &type, const std::string &language, bool hasUnicodeLayoutData,
    bool asciiCapable
)
{
  deskflow::osx::InputSourceRecord record;
  record.m_id = id;
  record.m_type = type;
  record.m_language = language;
  record.m_hasUnicodeLayoutData = hasUnicodeLayoutData;
  record.m_asciiCapable = asciiCapable;
  return record;
}

std::string keyboardLayoutType()
{
  return cfStringToString(kTISTypeKeyboardLayout);
}

} // namespace

void OSXKeyStateTests::initTestCase()
{
  m_arch.init();
  m_log.setFilter(LogLevel::Debug2);
}

void OSXKeyStateTests::mapModifiersFromOSX_OSXMask()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  OSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);

  KeyModifierMask outMask = 0;

  uint32_t shiftMask = 0 | kCGEventFlagMaskShift;
  outMask = keyState.mapModifiersFromOSX(shiftMask);
  QCOMPARE(outMask, KeyModifierShift);

  uint32_t ctrlMask = 0 | kCGEventFlagMaskControl;
  outMask = keyState.mapModifiersFromOSX(ctrlMask);
  QCOMPARE(outMask, KeyModifierControl);

  uint32_t altMask = 0 | kCGEventFlagMaskAlternate;
  outMask = keyState.mapModifiersFromOSX(altMask);
  QCOMPARE(outMask, KeyModifierAlt);

  uint32_t cmdMask = 0 | kCGEventFlagMaskCommand;
  outMask = keyState.mapModifiersFromOSX(cmdMask);
  QCOMPARE(outMask, KeyModifierSuper);

  uint32_t capsMask = 0 | kCGEventFlagMaskAlphaShift;
  outMask = keyState.mapModifiersFromOSX(capsMask);
  QCOMPARE(outMask, KeyModifierCapsLock);

  uint32_t numMask = 0 | kCGEventFlagMaskNumericPad;
  outMask = keyState.mapModifiersFromOSX(numMask);
  QCOMPARE(outMask, KeyModifierNumLock);
}

void OSXKeyStateTests::chooseTranslationInputSource_activeABC()
{
  std::vector<deskflow::osx::InputSourceRecord> records = {
      makeInputSourceRecord("com.apple.keylayout.ABC", keyboardLayoutType(), "en", true, true)
  };

  auto choice = deskflow::osx::chooseTranslationInputSource(records, 0);

  QVERIFY(choice.has_value());
  QCOMPARE(choice->m_index, std::size_t{0});
  QCOMPARE(choice->m_fallback, false);
  QCOMPARE(QString::fromStdString(choice->m_language), QString("en"));
}

void OSXKeyStateTests::chooseTranslationInputSource_inputMethodFallbackABC()
{
  std::vector<deskflow::osx::InputSourceRecord> records = {
      makeInputSourceRecord("org.youknowone.inputmethod.Gureum.han2", "Input Method", "ko", false, false),
      makeInputSourceRecord("com.apple.keylayout.ABC", keyboardLayoutType(), "en", true, true)
  };

  auto choice = deskflow::osx::chooseTranslationInputSource(records, 0);

  QVERIFY(choice.has_value());
  QCOMPARE(choice->m_index, std::size_t{1});
  QCOMPARE(choice->m_fallback, true);
  QCOMPARE(QString::fromStdString(choice->m_language), QString("en"));
}

void OSXKeyStateTests::chooseTranslationInputSource_preservesDvorak()
{
  std::vector<deskflow::osx::InputSourceRecord> records = {
      makeInputSourceRecord("com.apple.keylayout.Dvorak", keyboardLayoutType(), "en", true, true),
      makeInputSourceRecord("com.apple.keylayout.ABC", keyboardLayoutType(), "en", true, true)
  };

  auto choice = deskflow::osx::chooseTranslationInputSource(records, 0);

  QVERIFY(choice.has_value());
  QCOMPARE(choice->m_index, std::size_t{0});
  QCOMPARE(choice->m_fallback, false);
}

void OSXKeyStateTests::chooseTranslationInputSource_firstAsciiFallback()
{
  std::vector<deskflow::osx::InputSourceRecord> records = {
      makeInputSourceRecord("com.example.inputmethod.Korean", "Input Method", "ko", false, false),
      makeInputSourceRecord("com.example.keylayout.Ascii", keyboardLayoutType(), "en", true, true)
  };

  auto choice = deskflow::osx::chooseTranslationInputSource(records, 0);

  QVERIFY(choice.has_value());
  QCOMPARE(choice->m_index, std::size_t{1});
  QCOMPARE(choice->m_fallback, true);
  QCOMPARE(QString::fromStdString(choice->m_language), QString("en"));
}

void OSXKeyStateTests::mapKeyFromEvent_sidedCommandModifiers()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  OSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);

  auto verifyCommandKey = [&keyState](CGKeyCode virtualKey, KeyID expectedKeyID) {
    CGEventRef event = CGEventCreateKeyboardEvent(nullptr, virtualKey, true);
    QVERIFY(event != nullptr);
    CGEventSetType(event, kCGEventFlagsChanged);

    OSXKeyState::KeyIDs ids;
    const KeyButton button = keyState.mapKeyFromEvent(ids, nullptr, event);
    CFRelease(event);

    QCOMPARE(button, static_cast<KeyButton>(virtualKey + 1));
    QCOMPARE(ids.size(), std::size_t{1});
    QCOMPARE(ids[0], expectedKeyID);
  };

  verifyCommandKey(kVK_Command, kKeySuper_L);
  verifyCommandKey(kVK_RightCommand, kKeySuper_R);
}

void OSXKeyStateTests::handleModifierKeys_sidedCommandModifiers()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  RecordingOSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  void *target = reinterpret_cast<void *>(0x1);

  keyState.handleModifierKeys(
      target, kVK_Command, NX_COMMANDMASK | NX_DEVICELCMDKEYMASK, 0, KeyModifierSuper
  );
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{1});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_press, true);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, kKeySuper_L);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_mask, KeyModifierSuper);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_button, static_cast<KeyButton>(kVK_Command + 1));

  keyState.handleModifierKeys(
      target, kVK_RightCommand, NX_COMMANDMASK | NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK, KeyModifierSuper,
      KeyModifierSuper
  );
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{2});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_press, true);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, kKeySuper_R);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_mask, KeyModifierSuper);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_button, static_cast<KeyButton>(kVK_RightCommand + 1));

  keyState.handleModifierKeys(
      target, kVK_RightCommand, NX_COMMANDMASK | NX_DEVICELCMDKEYMASK, KeyModifierSuper, KeyModifierSuper
  );
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{3});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_press, false);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, kKeySuper_R);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_mask, KeyModifierSuper);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_button, static_cast<KeyButton>(kVK_RightCommand + 1));

  keyState.handleModifierKeys(target, kVK_Command, 0, KeyModifierSuper, 0);
  QCOMPARE(keyState.m_sentKeyEvents.size(), std::size_t{4});
  QCOMPARE(keyState.m_sentKeyEvents.back().m_press, false);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_key, kKeySuper_L);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_mask, 0);
  QCOMPARE(keyState.m_sentKeyEvents.back().m_button, static_cast<KeyButton>(kVK_Command + 1));
}

void OSXKeyStateTests::fakePollShift()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  OSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  keyState.updateKeyMap();

  keyState.fakeKeyDown(SHIFT_ID_L, 0, 1, "en");
  QVERIFY(isKeyPressed(keyState, SHIFT_BUTTON_L));

  keyState.fakeKeyUp(1);
  QVERIFY(!isKeyPressed(keyState, SHIFT_BUTTON_L));

  keyState.fakeKeyDown(SHIFT_ID_R, 0, 2, "en");
  QVERIFY(isKeyPressed(keyState, SHIFT_BUTTON_R));

  keyState.fakeKeyUp(2);
  QVERIFY(!isKeyPressed(keyState, SHIFT_BUTTON_R));
}

void OSXKeyStateTests::fakePollChar()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  OSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  keyState.updateKeyMap();

  keyState.fakeKeyDown(A_CHAR_ID, 0, 1, "en");
  QVERIFY(isKeyPressed(keyState, A_CHAR_BUTTON));

  keyState.fakeKeyUp(1);
  QVERIFY(!isKeyPressed(keyState, A_CHAR_BUTTON));

  // HACK: delete the key in case it was typed into a text editor.
  // we should really set focus to an invisible window.
  keyState.fakeKeyDown(kKeyBackSpace, 0, 2, "en");
  keyState.fakeKeyUp(2);
}

void OSXKeyStateTests::fakePollCharWithModifier()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  OSXKeyState keyState(&eventQueue, keyMap, {"en"}, true);
  keyState.updateKeyMap();

  keyState.fakeKeyDown(A_CHAR_ID, KeyModifierShift, 1, "en");
  QVERIFY(isKeyPressed(keyState, A_CHAR_BUTTON));

  keyState.fakeKeyUp(1);
  QVERIFY(!isKeyPressed(keyState, A_CHAR_BUTTON));

  // HACK: delete the key in case it was typed into a text editor.
  // we should really set focus to an invisible window.
  keyState.fakeKeyDown(kKeyBackSpace, 0, 2, "en");
  keyState.fakeKeyUp(2);
}

bool OSXKeyStateTests::isKeyPressed(const OSXKeyState &keyState, KeyButton button)
{
  // HACK: allow os to realize key state changes.
  Arch::sleep(.2);

  IKeyState::KeyButtonSet pressed;
  keyState.pollPressedKeys(pressed);

  IKeyState::KeyButtonSet::const_iterator it;
  for (it = pressed.begin(); it != pressed.end(); ++it) {
    LOG_DEBUG("checking key %d", *it);
    if (*it == button) {
      return true;
    }
  }
  // Synthetic macOS key events are not always reflected in GetKeys immediately.
  return keyState.getKeyState(button) != 0;
}

QTEST_MAIN(OSXKeyStateTests)
