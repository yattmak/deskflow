/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerProxyTests.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "client/Client.h"
#include "client/ModifierTranslation.h"
#include "client/ServerProxy.h"
#include "deskflow/AppUtil.h"
#include "deskflow/KeyMap.h"
#include "deskflow/KeyState.h"
#include "deskflow/ProtocolTypes.h"
#include "io/IStream.h"

#include <QTest>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {

class TestAppUtil : public AppUtil
{
public:
  int run() override
  {
    return 0;
  }

  std::vector<std::string> getKeyboardLayoutList() override
  {
    return {"en"};
  }

  std::string getCurrentLanguageCode() override
  {
    return "en";
  }
};

class FakeStream : public deskflow::IStream
{
public:
  void push(const std::string &bytes)
  {
    m_chunks.push_back(bytes);
  }

  void close() override
  {
    m_chunks.clear();
    m_inputShutdown = true;
  }

  uint32_t read(void *buffer, uint32_t size) override
  {
    if (m_inputShutdown || m_chunks.empty() || size == 0) {
      return 0;
    }

    auto &front = m_chunks.front();
    const size_t bytesToRead = std::min(static_cast<size_t>(size), front.size());
    if (buffer != nullptr) {
      std::memcpy(buffer, front.data(), bytesToRead);
    }

    front.erase(0, bytesToRead);
    if (front.empty()) {
      m_chunks.pop_front();
    }
    return static_cast<uint32_t>(bytesToRead);
  }

  void write(const void *, uint32_t) override
  {
  }

  void flush() override
  {
  }

  void shutdownInput() override
  {
    close();
  }

  void shutdownOutput() override
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<FakeStream *>(this);
  }

  bool isReady() const override
  {
    return !m_inputShutdown && !m_chunks.empty();
  }

  uint32_t getSize() const override
  {
    size_t total = 0;
    for (const auto &chunk : m_chunks) {
      total += chunk.size();
    }
    return static_cast<uint32_t>(std::min<size_t>(total, UINT32_MAX));
  }

private:
  std::deque<std::string> m_chunks;
  bool m_inputShutdown = false;
};

class RecordingEventQueue : public IEventQueue
{
public:
  ~RecordingEventQueue() override
  {
    for (const auto &event : m_addedEvents) {
      Event::deleteData(event);
    }
  }

  int loop() override
  {
    return 0;
  }

  void adoptBuffer(IEventQueueBuffer *) override
  {
  }

  bool getEvent(Event &, double = -1.0) override
  {
    return false;
  }

  bool dispatchEvent(const Event &event) override
  {
    const auto handler = m_handlers.find(HandlerKey{event.getType(), event.getTarget()});
    if (handler == m_handlers.end()) {
      return false;
    }

    const auto callback = handler->second;
    callback(event);
    return true;
  }

  void addEvent(Event &&event) override
  {
    m_addedEvents.emplace_back(std::move(event));
  }

  EventQueueTimer *newTimer(double, void *) override
  {
    return timer();
  }

  EventQueueTimer *newOneShotTimer(double, void *) override
  {
    return timer();
  }

  void deleteTimer(EventQueueTimer *) override
  {
  }

  void addHandler(EventTypes type, void *target, const EventHandler &handler) override
  {
    m_handlers[HandlerKey{type, target}] = handler;
  }

  void removeHandler(EventTypes type, void *target) override
  {
    m_handlers.erase(HandlerKey{type, target});
  }

  void removeHandlers(void *target) override
  {
    for (auto it = m_handlers.begin(); it != m_handlers.end();) {
      if (it->first.target == target) {
        it = m_handlers.erase(it);
      } else {
        ++it;
      }
    }
  }

  void waitForReady() const override
  {
  }

  void *getSystemTarget() override
  {
    return this;
  }

  EventQueueTimer *timer()
  {
    return reinterpret_cast<EventQueueTimer *>(&m_timerStorage);
  }

  const std::vector<Event> &addedEvents() const
  {
    return m_addedEvents;
  }

private:
  struct HandlerKey
  {
    EventTypes type;
    void *target;

    bool operator<(const HandlerKey &other) const
    {
      if (type != other.type) {
        return static_cast<uint32_t>(type) < static_cast<uint32_t>(other.type);
      }
      return std::less<void *>{}(target, other.target);
    }
  };

  int m_timerStorage = 0;
  std::map<HandlerKey, EventHandler> m_handlers;
  std::vector<Event> m_addedEvents;
};

// Exercise the real server-button/local-button bookkeeping without platform IO.
class RecordingKeyState : public KeyState
{
public:
  RecordingKeyState(IEventQueue *events, deskflow::KeyMap &keyMap) : KeyState(events, keyMap, {"en"}, false)
  {
  }

  int32_t pollActiveGroup() const override
  {
    return 0;
  }

  KeyModifierMask pollActiveModifiers() const override
  {
    return 0;
  }

  bool fakeCtrlAltDel() override
  {
    return false;
  }

  void pollPressedKeys(KeyButtonSet &) const override
  {
  }

  void getKeyMap(deskflow::KeyMap &) override
  {
  }

  void fakeKey(const Keystroke &stroke) override
  {
    if (stroke.m_type == Keystroke::KeyType::Button) {
      m_pressed[stroke.m_data.m_button.m_button] = stroke.m_data.m_button.m_press;
    }
  }

  bool physicallyDown(KeyButton button) const
  {
    const auto key = m_pressed.find(button);
    return key != m_pressed.end() && key->second;
  }

  void forward(deskflow::ModifierTranslation::Action action, const deskflow::ModifierTranslation::KeyEvent &event)
  {
    if (!event.shouldEmit) {
      return;
    }
    using enum deskflow::ModifierTranslation::Action;
    switch (action) {
    case Down:
      fakeKeyDown(event.key, event.mask, event.button, "en");
      break;
    case Repeat:
      fakeKeyRepeat(event.key, event.mask, 1, event.button, "en");
      break;
    case Up:
      // Just like Computer::keyUp, do not use event.key or event.mask.
      fakeKeyUp(event.button);
      break;
    }
  }

private:
  std::map<KeyButton, bool> m_pressed;
};

class TestServerProxy : public ServerProxy
{
public:
  using ServerProxy::ServerProxy;

  bool parseHandshakeMessageReturnsDisconnect(const uint8_t *code)
  {
    return parseHandshakeMessage(code) == ConnectionResult::Disconnect;
  }
};

Client *undereferenceableClient()
{
  // These paths must queue cleanup without calling through to Client.
  return reinterpret_cast<Client *>(0x1);
}

TestAppUtil &testAppUtil()
{
  static TestAppUtil util;
  return util;
}

const Client::DisconnectRequest *disconnectRequest(const RecordingEventQueue &events)
{
  if (events.addedEvents().size() != 1) {
    return nullptr;
  }
  return static_cast<const Client::DisconnectRequest *>(events.addedEvents().front().getDataObject());
}

} // namespace

void ServerProxyTests::initTestCase()
{
  (void)testAppUtil();
  m_log.setFilter(LogLevel::Level::Debug);
}

void ServerProxyTests::handleKeepAliveAlarm_timeout_queuesDisconnectRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  ServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, events.timer())));

  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto &event = events.addedEvents().front();
  QVERIFY(event.getType() == EventTypes::ClientDisconnectRequested);
  QCOMPARE(event.getTarget(), stream.getEventTarget());

  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Disconnect);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("server is not responding"));
}

void ServerProxyTests::handleData_incompleteMessage_queuesDisconnectRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  stream.push("DF");
  ServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto &event = events.addedEvents().front();
  QVERIFY(event.getType() == EventTypes::ClientDisconnectRequested);
  QCOMPARE(event.getTarget(), stream.getEventTarget());

  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Disconnect);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("incomplete message from server"));
}

void ServerProxyTests::parseHandshakeMessage_protocolError_queuesRefusalRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(proxy.parseHandshakeMessageReturnsDisconnect(reinterpret_cast<const uint8_t *>(kMsgEBad)));
  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Refuse);
  QVERIFY(request->refusalReason() == deskflow::core::ConnectionRefusal::ProtocolError);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("server reported a protocol error"));
}

void ServerProxyTests::modifierTranslation_generic_data()
{
  QTest::addColumn<quint32>("option");
  QTest::addColumn<quint32>("sourceKey");
  QTest::addColumn<quint32>("sourceMask");
  QTest::addColumn<quint32>("target");
  QTest::addColumn<quint32>("targetKey");
  QTest::addColumn<quint32>("targetMask");

  const OptionID options[] = {kOptionModifierMapForShift, kOptionModifierMapForControl, kOptionModifierMapForAlt,
                              kOptionModifierMapForMeta,  kOptionModifierMapForSuper,   kOptionModifierMapForAltGr};
  const KeyID keys[][2] = {{kKeyNone, kKeyNone},   {kKeyShift_L, kKeyShift_R}, {kKeyControl_L, kKeyControl_R},
                           {kKeyAlt_L, kKeyAlt_R}, {kKeyMeta_L, kKeyMeta_R},   {kKeySuper_L, kKeySuper_R},
                           {kKeyAltGr, kKeyAltGr}};
  const KeyModifierMask masks[] = {
      0, KeyModifierShift, KeyModifierControl, KeyModifierAlt, KeyModifierMeta, KeyModifierSuper, KeyModifierAltGr
  };
  for (KeyModifierID source = kKeyModifierIDShift; source < kKeyModifierIDLast; ++source) {
    for (KeyModifierID target = kKeyModifierIDNull; target < kKeyModifierIDLast; ++target) {
      for (int side = (source == kKeyModifierIDAltGr ? 1 : 0); side < 2; ++side) {
        const auto name = QStringLiteral("source-%1-target-%2-side-%3").arg(source).arg(target).arg(side);
        QTest::newRow(qPrintable(name)) << options[source - 1] << keys[source][side] << masks[source] << target
                                        << keys[target][side] << masks[target];
      }
    }
  }
}

void ServerProxyTests::modifierTranslation_generic()
{
  QFETCH(quint32, option);
  QFETCH(quint32, sourceKey);
  QFETCH(quint32, sourceMask);
  QFETCH(quint32, target);
  QFETCH(quint32, targetKey);
  QFETCH(quint32, targetMask);
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  const auto locks = KeyModifierCapsLock | KeyModifierNumLock | KeyModifierScrollLock | KeyModifierLevel5Lock;
  QCOMPARE(translation.setOption(option, target), Translation::OptionResult::Applied);
  QCOMPARE(translation.translateMask(sourceMask | locks), targetMask | locks);
  auto event = translation.translate(Down, sourceKey, sourceMask | locks, 1);
  QCOMPARE(event.key, targetKey);
  QCOMPARE(event.mask, targetMask | locks);
  event = translation.translate(Repeat, 'a', sourceMask | locks, 2);
  QCOMPARE(event.key, KeyID('a'));
  QCOMPARE(event.mask, targetMask | locks);
  event = translation.translate(Up, sourceKey, locks, 1);
  QCOMPARE(event.key, targetKey);
  QCOMPARE(event.mask, locks);
}

void ServerProxyTests::modifierTranslation_sideInheritance_data()
{
  QTest::addColumn<quint32>("genericOption");
  QTest::addColumn<quint32>("sideOption");
  QTest::addColumn<quint32>("sourceKey");
  QTest::addColumn<quint32>("otherKey");
  QTest::addColumn<quint32>("sourceMask");
  QTest::addColumn<quint32>("inheritedKey");
  QTest::newRow("meta-left") << kOptionModifierMapForMeta << kOptionModifierMapForMetaLeft << kKeyMeta_L << kKeyMeta_R
                             << KeyModifierMeta << kKeyControl_R;
  QTest::newRow("meta-right") << kOptionModifierMapForMeta << kOptionModifierMapForMetaRight << kKeyMeta_R << kKeyMeta_L
                              << KeyModifierMeta << kKeyControl_L;
  QTest::newRow("super-left") << kOptionModifierMapForSuper << kOptionModifierMapForSuperLeft << kKeySuper_L
                              << kKeySuper_R << KeyModifierSuper << kKeyControl_R;
  QTest::newRow("super-right") << kOptionModifierMapForSuper << kOptionModifierMapForSuperRight << kKeySuper_R
                               << kKeySuper_L << KeyModifierSuper << kKeyControl_L;
}

void ServerProxyTests::modifierTranslation_sideInheritance()
{
  QFETCH(quint32, genericOption);
  QFETCH(quint32, sideOption);
  QFETCH(quint32, sourceKey);
  QFETCH(quint32, otherKey);
  QFETCH(quint32, sourceMask);
  QFETCH(quint32, inheritedKey);
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  // Setting the override first must not make option order significant.
  QCOMPARE(translation.setOption(sideOption, kKeyHangul), Translation::OptionResult::Applied);
  QCOMPARE(translation.setOption(genericOption, kKeyModifierIDControl), Translation::OptionResult::Applied);
  QCOMPARE(translation.translateMask(sourceMask), KeyModifierMask(0));
  auto event = translation.translate(Down, otherKey, sourceMask, 1);
  QCOMPARE(event.key, inheritedKey);
  QCOMPARE(event.mask, KeyModifierControl);
  translation.translate(Up, otherKey, 0, 1);
  event = translation.translate(Down, sourceKey, sourceMask, 2);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMask(0));
  translation.translate(Up, sourceKey, 0, 2);

  QCOMPARE(translation.setOption(genericOption, kKeyModifierIDAltGr), Translation::OptionResult::Applied);
  event = translation.translate(Down, otherKey, sourceMask, 1);
  QCOMPARE(event.key, kKeyAltGr);
  QCOMPARE(event.mask, KeyModifierAltGr);
  translation.translate(Up, otherKey, 0, 1);
  event = translation.translate(Down, sourceKey, sourceMask, 2);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMask(0));
}

void ServerProxyTests::modifierTranslation_hangulDownRepeatUp_consumesMask()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuperRight, kKeyHangul);
  auto event = translation.translate(Down, kKeySuper_R, KeyModifierSuper | KeyModifierCapsLock, 42);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierCapsLock);
  QCOMPARE(event.button, KeyButton(42));
  event = translation.translate(Repeat, kKeySuper_R, KeyModifierSuper | KeyModifierCapsLock, 42);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierCapsLock);
  event = translation.translate(Down, 'a', KeyModifierSuper | KeyModifierCapsLock, 43);
  QCOMPARE(event.key, KeyID('a'));
  QCOMPARE(event.mask, KeyModifierCapsLock);
  event = translation.translate(Repeat, 'a', KeyModifierSuper | KeyModifierCapsLock, 43);
  QCOMPARE(event.mask, KeyModifierCapsLock);
  // A pre-transition mask must not bring Super back on the release event.
  event = translation.translate(Up, kKeyNone, KeyModifierSuper | KeyModifierCapsLock, 42);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierCapsLock);
  QCOMPARE(event.button, KeyButton(42));
  event = translation.translate(Down, kKeySuper_L, KeyModifierSuper, 41);
  QCOMPARE(event.key, kKeySuper_L);
  QCOMPARE(event.mask, KeyModifierSuper);
}

void ServerProxyTests::modifierTranslation_altGrMaskOmission_isRespected()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1);
  // macOS deliberately omits Super when Option was used to compose a character.
  auto event = translation.translate(Down, 'a', 0, 2);
  QCOMPARE(event.mask, KeyModifierMask(0));
  translation.setOption(kOptionModifierMapForAltGr, kKeyModifierIDControl);
  event = translation.translate(Repeat, 'a', KeyModifierAltGr, 2);
  QCOMPARE(event.mask, KeyModifierControl);
  translation.translate(Up, kKeySuper_L, 0, 1);
  event = translation.translate(Down, kKeyAltGr, 0, 3);
  QCOMPARE(event.key, kKeyControl_R);
  QCOMPARE(event.mask, KeyModifierMask(0));
  event = translation.translate(Repeat, kKeyAltGr, KeyModifierAltGr, 3);
  QCOMPARE(event.key, kKeyControl_R);
  QCOMPARE(event.mask, KeyModifierControl);
  event = translation.translate(Up, kKeyAltGr, 0, 3);
  QCOMPARE(event.mask, KeyModifierMask(0));
}

void ServerProxyTests::modifierTranslation_bothHeldRelease_data()
{
  QTest::addColumn<bool>("releaseLeft");
  QTest::addColumn<bool>("rawBitSet");
  for (const bool left : {false, true}) {
    for (const bool bit : {false, true}) {
      const auto name = QStringLiteral("release-left-%1-mask-%2").arg(left).arg(bit);
      QTest::newRow(qPrintable(name)) << left << bit;
    }
  }
}

void ServerProxyTests::modifierTranslation_bothHeldRelease()
{
  QFETCH(bool, releaseLeft);
  QFETCH(bool, rawBitSet);
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForMetaRight, kKeyHangul);
  translation.translate(Down, kKeyMeta_L, KeyModifierMeta, 1);
  auto event = translation.translate(Down, kKeyMeta_R, KeyModifierMeta, 2);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMeta);
  event = translation.translate(
      Up, releaseLeft ? kKeyMeta_L : kKeyMeta_R, rawBitSet ? KeyModifierMeta : 0, releaseLeft ? 1 : 2
  );
  QCOMPARE(event.key, releaseLeft ? kKeyMeta_L : kKeyHangul);
  QCOMPARE(event.mask, releaseLeft ? KeyModifierMask(0) : KeyModifierMeta);
  event = translation.translate(Repeat, 'a', KeyModifierMeta, 3);
  QCOMPARE(event.mask, releaseLeft ? KeyModifierMask(0) : KeyModifierMeta);
  event = translation.translate(Up, releaseLeft ? kKeyMeta_R : kKeyMeta_L, 0, releaseLeft ? 2 : 1);
  QCOMPARE(event.mask, KeyModifierMask(0));
}

void ServerProxyTests::modifierTranslation_bothOverrides_composesMasks()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForMetaLeft, kKeyModifierIDControl);
  translation.setOption(kOptionModifierMapForMetaRight, kKeyModifierIDAlt);
  auto event = translation.translate(Down, kKeyMeta_L, KeyModifierMeta, 1);
  QCOMPARE(event.key, kKeyControl_L);
  QCOMPARE(event.mask, KeyModifierControl);
  event = translation.translate(Down, kKeyMeta_R, KeyModifierMeta, 2);
  QCOMPARE(event.key, kKeyAlt_R);
  QCOMPARE(event.mask, KeyModifierControl | KeyModifierAlt);
  event = translation.translate(Repeat, 'a', KeyModifierMeta, 3);
  QCOMPARE(event.mask, KeyModifierControl | KeyModifierAlt);
  event = translation.translate(Up, kKeyMeta_L, 0, 1);
  QCOMPARE(event.mask, KeyModifierAlt);
  event = translation.translate(Repeat, 'a', KeyModifierMeta, 3);
  QCOMPARE(event.mask, KeyModifierAlt);
  event = translation.translate(Up, kKeyMeta_R, KeyModifierMeta, 2);
  QCOMPARE(event.mask, KeyModifierMask(0));
}

void ServerProxyTests::modifierTranslation_sharedTarget_preservesOtherSource()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForControl, kKeyModifierIDShift);
  translation.setOption(kOptionModifierMapForMeta, kKeyModifierIDShift);
  translation.setOption(kOptionModifierMapForMetaRight, kKeyHangul);
  translation.translate(Down, kKeyControl_L, KeyModifierControl, 1);
  const auto mask = KeyModifierControl | KeyModifierMeta;
  auto event = translation.translate(Down, kKeyMeta_R, mask, 2);
  QCOMPARE(event.mask, KeyModifierShift);
  event = translation.translate(Repeat, 'a', mask, 3);
  QCOMPARE(event.mask, KeyModifierShift);
  event = translation.translate(Up, kKeyMeta_R, mask, 2);
  QCOMPARE(event.mask, KeyModifierShift);
}

void ServerProxyTests::modifierTranslation_invalidTarget_keepsMapping()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForMeta, kKeyModifierIDControl);
  translation.setOption(kOptionModifierMapForMetaRight, kKeyHangul);
  const uint32_t invalid[] = {7, 8, 31, 127, 0x80, 0xd800, 0xe123, 0xffff, 0x110000, UINT32_MAX};
  for (const auto target : invalid) {
    QCOMPARE(translation.setOption(kOptionModifierMapForMetaRight, target), Translation::OptionResult::Invalid);
    QCOMPARE(translation.setOption(kOptionModifierMapForMeta, target), Translation::OptionResult::Invalid);
    auto event = translation.translate(Down, kKeyMeta_R, KeyModifierMeta, 1);
    QCOMPARE(event.key, kKeyHangul);
    QCOMPARE(event.mask, KeyModifierMask(0));
    translation.translate(Up, kKeyMeta_R, 0, 1);
    event = translation.translate(Down, kKeyMeta_L, KeyModifierMeta, 2);
    QCOMPARE(event.key, kKeyControl_L);
    QCOMPARE(event.mask, KeyModifierControl);
    translation.translate(Up, kKeyMeta_L, 0, 2);
  }
  QCOMPARE(translation.setOption(kOptionHeartbeat, UINT32_MAX), Translation::OptionResult::Unhandled);
}

void ServerProxyTests::modifierTranslation_namedKeysAndAliases()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  for (auto entry = kKeyNameMap; entry->m_name != nullptr; ++entry) {
    if (entry->m_id == kKeyCapsLock || entry->m_id == kKeyNumLock || entry->m_id == kKeyScrollLock) {
      QCOMPARE(translation.setOption(kOptionModifierMapForSuperRight, entry->m_id), Translation::OptionResult::Invalid);
      continue;
    }
    QCOMPARE(translation.setOption(kOptionModifierMapForSuperRight, entry->m_id), Translation::OptionResult::Applied);
    const auto event = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 1);
    QCOMPARE(event.key, entry->m_id);
    translation.translate(Up, kKeySuper_R, 0, 1);
  }
  for (const auto name : {"Hangul", "Haen", "Hanja"}) {
    KeyID key = kKeyNone;
    QVERIFY(deskflow::KeyMap::parseKey(name, key));
    QCOMPARE(key, std::string(name) == "Hanja" ? kKeyHanja : kKeyHangul);
  }
  QCOMPARE(deskflow::KeyMap::formatKey(kKeyHangul, 0), std::string("Hangul"));
}

void ServerProxyTests::modifierTranslation_optionsChangedWhileHeld_retainsPressTarget()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuperRight, kKeyHangul);
  translation.translate(Down, kKeySuper_R, KeyModifierSuper, 1);
  translation.setOption(kOptionModifierMapForSuperRight, kKeyModifierIDAlt);
  auto event = translation.translate(Repeat, kKeySuper_R, KeyModifierSuper, 1);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMask(0));
  event = translation.translate(Up, kKeySuper_R, KeyModifierSuper, 1);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMask(0));
  event = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 1);
  QCOMPARE(event.key, kKeyAlt_R);
  QCOMPARE(event.mask, KeyModifierAlt);
}

void ServerProxyTests::modifierTranslation_deactivate_releasesKeysAndClearsSides()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuperRight, kKeyHangul);
  translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1);
  translation.translate(Down, kKeySuper_R, KeyModifierSuper, 2);
  const auto releases = translation.releaseActiveKeys();
  QCOMPARE(releases.size(), size_t(2));
  QCOMPARE(releases[0].key, kKeySuper_L);
  QCOMPARE(releases[0].button, KeyButton(1));
  QCOMPARE(releases[0].mask, KeyModifierMask(0));
  QCOMPARE(releases[1].key, kKeyHangul);
  QCOMPARE(releases[1].button, KeyButton(2));
  QCOMPARE(releases[1].mask, KeyModifierMask(0));
  QVERIFY(translation.releaseActiveKeys().empty());
  // A mask-only enter cannot distinguish Super_L from the Hangul source.
  QCOMPARE(translation.translateMask(KeyModifierSuper), KeyModifierMask(0));
  const auto event = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 2);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMask(0));
}

void ServerProxyTests::modifierTranslation_reset_restoresInheritance()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForMeta, kKeyModifierIDControl);
  translation.setOption(kOptionModifierMapForMetaRight, kKeyHangul);
  translation.setOption(kOptionModifierMapForSuperLeft, kKeyHanja);
  translation.translate(Down, kKeyMeta_R, KeyModifierMeta, 1);
  translation.translate(Down, kKeySuper_L, KeyModifierMeta | KeyModifierSuper, 2);
  const auto releases = translation.releaseActiveKeys();
  QCOMPARE(releases.size(), size_t(2));
  QCOMPARE(releases[0].key, kKeyHangul);
  QCOMPARE(releases[1].key, kKeyHanja);
  translation.resetOptions();
  QVERIFY(translation.releaseActiveKeys().empty());
  QCOMPARE(translation.translateMask(KeyModifierMeta | KeyModifierSuper), KeyModifierMeta | KeyModifierSuper);
  auto event = translation.translate(Down, kKeyMeta_R, KeyModifierMeta, 1);
  QCOMPARE(event.key, kKeyMeta_R);
  QCOMPARE(event.mask, KeyModifierMeta);
  translation.translate(Up, kKeyMeta_R, 0, 1);
  translation.setOption(kOptionModifierMapForMeta, kKeyModifierIDAlt);
  event = translation.translate(Down, kKeyMeta_R, KeyModifierMeta, 1);
  QCOMPARE(event.key, kKeyAlt_R);
  QCOMPARE(event.mask, KeyModifierAlt);
}

void ServerProxyTests::modifierTranslation_sharedPhysicalTarget_data()
{
  QTest::addColumn<quint32>("target");
  QTest::addColumn<bool>("releaseOwnerFirst");
  QTest::newRow("control-owner-first") << kKeyControl_L << true;
  QTest::newRow("control-other-first") << kKeyControl_L << false;
  QTest::newRow("hangul-owner-first") << kKeyHangul << true;
  QTest::newRow("hangul-other-first") << kKeyHangul << false;
}

void ServerProxyTests::modifierTranslation_sharedPhysicalTarget()
{
  QFETCH(quint32, target);
  QFETCH(bool, releaseOwnerFirst);
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuperLeft, target);
  translation.setOption(kOptionModifierMapForSuperRight, target);
  RecordingEventQueue events;
  deskflow::KeyMap keyMap;
  deskflow::KeyMap::KeyItem item;
  item.m_id = target;
  item.m_button = 42;
  deskflow::KeyMap::initModifierKey(item);
  keyMap.addKeyEntry(item);
  keyMap.finish();
  RecordingKeyState keys(&events, keyMap);

  auto event = translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.button, KeyButton(1));
  keys.forward(Down, event);
  QVERIFY(keys.physicallyDown(42));
  QCOMPARE(keys.getKeyState(42), 1);
  event = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 2);
  QVERIFY(!event.shouldEmit);
  keys.forward(Down, event);
  QCOMPARE(keys.getKeyState(42), 1);
  event = translation.translate(
      Up, releaseOwnerFirst ? kKeySuper_L : kKeySuper_R, KeyModifierSuper, releaseOwnerFirst ? 1 : 2
  );
  QVERIFY(!event.shouldEmit);
  keys.forward(Up, event);
  QVERIFY(keys.physicallyDown(42));
  QVERIFY(keys.isKeyDown(42));
  if (target == kKeyControl_L) {
    QCOMPARE(keys.getActiveModifiers(), KeyModifierControl);
  }
  event = translation.translate(
      Repeat, releaseOwnerFirst ? kKeySuper_R : kKeySuper_L, KeyModifierSuper, releaseOwnerFirst ? 2 : 1
  );
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.button, KeyButton(1));
  keys.forward(Repeat, event);
  QVERIFY(keys.physicallyDown(42));
  event = translation.translate(Up, kKeyNone, 0, releaseOwnerFirst ? 2 : 1);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.button, KeyButton(1));
  keys.forward(Up, event);
  QVERIFY(!keys.physicallyDown(42));
  QVERIFY(!keys.isKeyDown(42));
  QCOMPARE(keys.getActiveModifiers(), KeyModifierMask(0));
  QVERIFY(translation.releaseActiveKeys().empty());
}

void ServerProxyTests::modifierTranslation_sharedOwnerButton_reuseDoesNotAlias()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuperLeft, kKeyControl_L);
  translation.setOption(kOptionModifierMapForSuperRight, kKeyControl_L);
  const auto owner = translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1);
  translation.translate(Down, kKeySuper_R, KeyModifierSuper, 2);
  QVERIFY(!translation.translate(Up, kKeySuper_L, KeyModifierSuper, 1).shouldEmit);

  // A new key using the physically released button must not replace Control's
  // KeyState entry, even though the other source still owns that output slot.
  auto event = translation.translate(Down, 'a', KeyModifierSuper, 1);
  QVERIFY(event.shouldEmit);
  QVERIFY(event.button != owner.button);
  const auto ordinaryButton = event.button;
  translation.resetOptions();
  event = translation.translate(Repeat, 'a', KeyModifierSuper, 1);
  QCOMPARE(event.button, ordinaryButton);
  event = translation.translate(Up, 'a', KeyModifierSuper, 1);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.button, ordinaryButton);
  translation.setOption(kOptionModifierMapForSuperLeft, kKeyAlt_L);
  event = translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.key, kKeyAlt_L);
  QVERIFY(event.button != owner.button);
  const auto altButton = event.button;
  event = translation.translate(Up, kKeySuper_R, KeyModifierSuper, 2);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.key, kKeyControl_L);
  QCOMPARE(event.button, owner.button);
  event = translation.translate(Up, kKeySuper_L, 0, 1);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.button, altButton);
  QVERIFY(translation.releaseActiveKeys().empty());
}

void ServerProxyTests::modifierTranslation_sharedPhysicalTarget_deactivateReleasesOnce()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuperLeft, kKeyControl_L);
  translation.setOption(kOptionModifierMapForSuperRight, kKeyControl_L);
  const auto owner = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 2);
  QVERIFY(!translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1).shouldEmit);
  const auto releases = translation.releaseActiveKeys();
  QCOMPARE(releases.size(), size_t(1));
  QVERIFY(releases[0].shouldEmit);
  QCOMPARE(releases[0].key, kKeyControl_L);
  QCOMPARE(releases[0].button, owner.button);
  QVERIFY(translation.releaseActiveKeys().empty());
  QVERIFY(!translation.translate(Up, kKeyNone, 0, 2).shouldEmit);
}

void ServerProxyTests::modifierTranslation_resetWhileHeld_preservesPressTargets()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuper, kKeyModifierIDControl);
  translation.setOption(kOptionModifierMapForSuperRight, kKeyHangul);
  const auto down = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 1);
  QCOMPARE(down.key, kKeyHangul);
  translation.resetOptions();
  QCOMPARE(translation.translateMask(KeyModifierSuper), KeyModifierMask(0));
  // Reproduce Server::sendOptions: reset followed by a complete options list.
  translation.setOption(kOptionModifierMapForSuper, kKeyModifierIDControl);
  translation.setOption(kOptionModifierMapForSuperRight, kKeyHangul);
  auto event = translation.translate(Down, 'a', KeyModifierSuper, 2);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.mask, KeyModifierMask(0));
  translation.translate(Up, 'a', KeyModifierSuper, 2);
  // Removing the override also takes effect only at the next physical press.
  translation.resetOptions();
  translation.setOption(kOptionModifierMapForSuper, kKeyModifierIDControl);
  event = translation.translate(Down, 'a', KeyModifierSuper, 2);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.mask, KeyModifierMask(0));
  translation.translate(Up, 'a', KeyModifierSuper, 2);
  event = translation.translate(Repeat, kKeySuper_R, KeyModifierSuper, 1);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.button, down.button);
  event = translation.translate(Up, kKeyNone, KeyModifierSuper, 1);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMask(0));
  QCOMPARE(event.button, down.button);
  event = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 1);
  QCOMPARE(event.key, kKeyControl_R);
  QCOMPARE(event.mask, KeyModifierControl);
}

void ServerProxyTests::modifierTranslation_resetSharedTarget_preservesOwner()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuperLeft, kKeyControl_L);
  translation.setOption(kOptionModifierMapForSuperRight, kKeyControl_L);
  const auto owner = translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1);
  translation.translate(Down, kKeySuper_R, KeyModifierSuper, 2);
  translation.resetOptions();
  translation.setOption(kOptionModifierMapForSuperRight, kKeyHangul);
  auto event = translation.translate(Up, kKeySuper_L, KeyModifierSuper, 1);
  QVERIFY(!event.shouldEmit);
  QCOMPARE(event.mask, KeyModifierControl);
  event = translation.translate(Up, kKeySuper_R, 0, 2);
  QVERIFY(event.shouldEmit);
  QCOMPARE(event.key, kKeyControl_L);
  QCOMPARE(event.button, owner.button);
  event = translation.translate(Down, kKeySuper_R, KeyModifierSuper, 2);
  QCOMPARE(event.key, kKeyHangul);
  QCOMPARE(event.mask, KeyModifierMask(0));
}

void ServerProxyTests::modifierTranslation_reenterAmbiguousMask_avoidsShortcut()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForSuper, kKeyModifierIDControl);
  translation.setOption(kOptionModifierMapForSuperRight, kKeyHangul);
  translation.translate(Down, kKeySuper_R, KeyModifierSuper, 1);
  translation.releaseActiveKeys();
  // CEnter contains only a mask. The source side/ordinary target cannot be
  // reconstructed, so do not guess Control and turn the next key into Ctrl+A.
  QCOMPARE(translation.translateMask(KeyModifierSuper), KeyModifierMask(0));
  auto event = translation.translate(Down, 'a', KeyModifierSuper, 2);
  QCOMPARE(event.mask, KeyModifierMask(0));
  translation.releaseActiveKeys();
  // Unambiguous generic remapping still works without an observed key down.
  translation.resetOptions();
  translation.setOption(kOptionModifierMapForSuper, kKeyModifierIDControl);
  QCOMPARE(translation.translateMask(KeyModifierSuper), KeyModifierControl);
  // Different keys that generate the same modifier mask are also unambiguous.
  translation.setOption(kOptionModifierMapForSuperRight, kKeyControl_L);
  QCOMPARE(translation.translateMask(KeyModifierSuper), KeyModifierControl);
  event = translation.translate(Down, kKeySuper_L, KeyModifierSuper, 1);
  QCOMPARE(event.key, kKeyControl_L);
  QCOMPARE(event.mask, KeyModifierControl);
}

void ServerProxyTests::modifierTranslation_lockTargets_areRejected()
{
  using Translation = deskflow::ModifierTranslation;
  using enum Translation::Action;
  Translation translation;
  translation.setOption(kOptionModifierMapForMeta, kKeyModifierIDControl);
  translation.setOption(kOptionModifierMapForMetaRight, kKeyHangul);
  for (const auto lock : {kKeyCapsLock, kKeyNumLock, kKeyScrollLock}) {
    QCOMPARE(translation.setOption(kOptionModifierMapForMeta, lock), Translation::OptionResult::Invalid);
    QCOMPARE(translation.setOption(kOptionModifierMapForMetaRight, lock), Translation::OptionResult::Invalid);
    auto event = translation.translate(Down, kKeyMeta_R, KeyModifierMeta, 1);
    QCOMPARE(event.key, kKeyHangul);
    QCOMPARE(event.mask, KeyModifierMask(0));
    translation.translate(Up, kKeyMeta_R, 0, 1);
    event = translation.translate(Down, kKeyMeta_L, KeyModifierMeta, 2);
    QCOMPARE(event.key, kKeyControl_L);
    QCOMPARE(event.mask, KeyModifierControl);
    translation.translate(Up, kKeyMeta_L, 0, 2);
  }
}

QTEST_MAIN(ServerProxyTests)
