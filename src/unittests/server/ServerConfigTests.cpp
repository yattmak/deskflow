/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigTests.h"

#include "server/Config.h"

#include "common/KeyboardModifier.h"

#include <sstream>

class OnlySystemFilter : public InputFilter::Condition
{
public:
  Condition *clone() const override
  {
    return new OnlySystemFilter();
  }
  std::string format() const override
  {
    return "";
  }

  InputFilter::FilterStatus match(const Event &ev) override
  {
    return ev.getType() == EventTypes::System ? InputFilter::FilterStatus::Activate
                                              : InputFilter::FilterStatus::NoMatch;
  }
};

using namespace deskflow::server;

void ServerConfigTests::initTestCase()
{
  m_arch.init();
  QVERIFY(m_settingsDirectory.isValid());
  m_previousSettingsFile = Settings::settingsFile();
  Settings::setSettingsFile(m_settingsDirectory.filePath("server-config.conf"));
}

void ServerConfigTests::cleanupTestCase()
{
  if (!m_previousSettingsFile.isEmpty())
    Settings::setSettingsFile(m_previousSettingsFile);
}

void ServerConfigTests::equalityCheck()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a != b);

  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);

  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a.addOption(std::string(), kOptionClipboardSharing, 1));
  QVERIFY(b.addOption(std::string(), kOptionClipboardSharing, 1));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  b.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  /* TODO Fix linking to the proper libs
  NetworkAddress addr1("localhost", 8080);
  addr1.resolve();
  NetworkAddress addr2("localhost", 8080);
  addr2.resolve();
  a.setDeskflowAddress(addr1);
  b.setDeskflowAddress(addr2);
  */
  QVERIFY(a == b);
}

void ServerConfigTests::equalityCheck_diff_options()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 0));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_alias()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  QVERIFY(a != b);

  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasB"));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_filters()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a != b);
}

// TODO FIX
/*
void ServerConfigTests::equalityCheck_diff_address()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  a.setDeskflowAddress(NetworkAddress(8000));
  b.setDeskflowAddress(NetworkAddress(9000));
  QVERIFY(a != b);
}
*/

void ServerConfigTests::equalityCheck_diff_neighbours1()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);
  QVERIFY(b != a);
}

void ServerConfigTests::equalityCheck_diff_neighbours2()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.25f, "computerB", 0.25f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_neighbours3()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerC", 0.5f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::modifierOptionsFromSettings()
{
  const auto name = QStringLiteral("modifier-protocol");
  Settings::setValue(Settings::Computer::ModifierMeta.arg(name), "ctrl");
  Settings::setValue(Settings::Computer::ModifierMetaRight.arg(name), "hangul");
  Settings::setValue(Settings::Computer::ModifierSuperLeft.arg(name), "none");
  Settings::setValue(Settings::Computer::ModifierSuperRight.arg(name), "hanja");
  Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  const auto *options = config.getOptions(name.toStdString());
  QVERIFY(options);
  QCOMPARE(options->at(kOptionModifierMapForAltGr), kKeyModifierIDAltGr);
  QCOMPARE(options->at(kOptionModifierMapForMeta), kKeyModifierIDControl);
  QVERIFY(!options->contains(kOptionModifierMapForMetaLeft));
  QCOMPARE(options->at(kOptionModifierMapForMetaRight), kKeyHangul);
  QCOMPARE(options->at(kOptionModifierMapForSuperLeft), kKeyModifierIDNull);
  QCOMPARE(options->at(kOptionModifierMapForSuperRight), kKeyHanja);

  Settings::setValue(Settings::Computer::ModifierMetaLeft.arg(name), "meta");
  Settings::setValue(Settings::Computer::ModifierMetaRight.arg(name), "Return");
  Config explicitTargets(nullptr);
  QVERIFY(explicitTargets.addComputer(name.toStdString()));
  options = explicitTargets.getOptions(name.toStdString());
  QCOMPARE(options->at(kOptionModifierMapForMetaLeft), kKeyModifierIDMeta);
  QCOMPARE(options->at(kOptionModifierMapForMetaRight), kKeyReturn);
}

void ServerConfigTests::modifierTargets_data()
{
  QTest::addColumn<QString>("target");
  QTest::addColumn<int>("expected");
  QTest::newRow("shift") << QString("shift") << int(kKeyModifierIDShift);
  QTest::newRow("ctrl") << QString("ctrl") << int(kKeyModifierIDControl);
  QTest::newRow("alt") << QString("alt") << int(kKeyModifierIDAlt);
  QTest::newRow("meta") << QString("meta") << int(kKeyModifierIDMeta);
  QTest::newRow("super") << QString("super") << int(kKeyModifierIDSuper);
  QTest::newRow("altgr") << QString("altgr") << int(kKeyModifierIDAltGr);
  QTest::newRow("none") << QString("none") << int(kKeyModifierIDNull);
  QTest::newRow("hangul") << QString("Hangul") << int(kKeyHangul);
  QTest::newRow("haen") << QString("haen") << int(kKeyHangul);
  QTest::newRow("kc-haen") << QString("KC_HAEN") << int(kKeyHangul);
  QTest::newRow("hanja") << QString("Hanja") << int(kKeyHanja);
  QTest::newRow("kc-haej") << QString("kc_haej") << int(kKeyHanja);
  QTest::newRow("named-key") << QString("Return") << int(kKeyReturn);
  QTest::newRow("left-target") << QString("Meta_L") << int(kKeyMeta_L);
  QTest::newRow("right-target") << QString("Super_R") << int(kKeySuper_R);
  QTest::newRow("character") << QString("x") << int('x');
  QTest::newRow("unicode-key") << QString("\\uEF31") << int(kKeyHangul);
}

void ServerConfigTests::modifierTargets()
{
  QFETCH(QString, target);
  QFETCH(int, expected);
  std::istringstream stream;
  ConfigReadContext context(stream);
  QCOMPARE(context.parseModifierKey(target.toStdString()), expected);
}

void ServerConfigTests::invalidModifierTarget()
{
  std::istringstream stream;
  ConfigReadContext context(stream);
  QVERIFY_THROWS_EXCEPTION(ServerConfigReadException, context.parseModifierKey("not_a_key"));
  QVERIFY_THROWS_EXCEPTION(ServerConfigReadException, context.parseModifierKey(""));
  // Explicit key values in the wire modifier-ID range are ambiguous.
  QVERIFY_THROWS_EXCEPTION(ServerConfigReadException, context.parseModifierKey("\\u0005"));
  for (const auto *target : {"CapsLock", "NumLock", "ScrollLock", "\\uEFE5", "\\uEF7F", "\\uEF14"})
    QVERIFY_THROWS_EXCEPTION(ServerConfigReadException, context.parseModifierKey(target));
}

void ServerConfigTests::legacyScreenModifierOptions()
{
  Settings::setValue(Settings::Computer::Name.arg("legacyKnown"), "legacyKnown");
  Settings::setValue(Settings::Computer::ModifierMeta.arg("legacyKnown"), "alt");
  Settings::setValue(Settings::Computer::ModifierMetaRight.arg("legacyKnown"), "hangul");
  Settings::setValue(Settings::Computer::SwitchCornerSize.arg("legacyKnown"), 17);
  Settings::setValue(Settings::Server::EnableClipboard, false);
  Settings::setValue(Settings::Server::ClipboardSize, 123);
  std::istringstream stream(
      "section: screens\n"
      "legacyKnown:\n"
      "  meta = ctrl\n"
      "  metaRight = Hanja\n"
      "  superLeft = none\n"
      "  switchCornerSize = 999\n"
      "legacyNew:\n"
      "  superRight = Return\n"
      "  altgr = altgr\n"
      "end\n"
      "section: links\n"
      "legacyKnown:\n"
      "  right = legacyNew\n"
      "end\n"
      "section: options\n"
      "  clipboardSharing = true\n"
      "end\n"
  );
  Config config(nullptr);
  ConfigReadContext context(stream);
  config.read(context);
  QVERIFY(config.isComputer("legacyNew"));
  QCOMPARE(config.getNeighbor("legacyKnown", Direction::Right, 0.5f, nullptr), std::string("legacyNew"));
  const auto *known = config.getOptions("legacyKnown");
  QVERIFY(known);
  QCOMPARE(known->at(kOptionModifierMapForMeta), kKeyModifierIDControl);
  QCOMPARE(known->at(kOptionModifierMapForMetaRight), kKeyHanja);
  QVERIFY(!known->contains(kOptionModifierMapForMetaLeft));
  QCOMPARE(known->at(kOptionModifierMapForSuperLeft), kKeyModifierIDNull);
  QCOMPARE(known->at(kOptionComputerSwitchCornerSize), 17);
  const auto *added = config.getOptions("legacyNew");
  QVERIFY(added);
  QCOMPARE(added->at(kOptionModifierMapForSuperRight), kKeyReturn);
  QCOMPARE(added->at(kOptionModifierMapForAltGr), kKeyModifierIDAltGr);
  const auto *global = config.getOptions("");
  QVERIFY(global);
  QCOMPARE(global->at(kOptionClipboardSharing), 0);
  QCOMPARE(global->at(kOptionClipboardSharingSize), 123 * 1024);
  // Reading an external file must not mutate the GUI/general settings.
  QCOMPARE(Settings::value(Settings::Computer::ModifierMeta.arg("legacyKnown")).toString(), "alt");
}

void ServerConfigTests::legacyScreenSectionTermination_data()
{
  QTest::addColumn<QString>("text");
  QTest::addColumn<bool>("complete");
  QTest::newRow("end-at-eof") << QString("section: screens\nlegacyEof:\nmetaRight=hangul\nend") << true;
  QTest::newRow("comments-whitespace-and-delimiters")
      << QString("section: screens\n  legacyEof: # computer\n\n\tmetaRight \t= hangul # target\nend\n") << true;
  QTest::newRow("missing-end-at-eof") << QString("section: screens\nlegacyEof:\nmetaRight=hangul") << false;
  QTest::newRow("missing-end-after-newline") << QString("section: screens\nlegacyEof:\nmetaRight=hangul\n") << false;
}

void ServerConfigTests::legacyScreenSectionTermination()
{
  QFETCH(QString, text);
  QFETCH(bool, complete);
  std::istringstream stream(text.toStdString());
  ConfigReadContext context(stream);
  Config config(nullptr);
  if (!complete) {
    QVERIFY_THROWS_EXCEPTION(ServerConfigReadException, config.read(context));
    return;
  }
  config.read(context);
  QVERIFY(stream.eof());
  const auto *options = config.getOptions("legacyEof");
  QVERIFY(options);
  QCOMPARE(options->at(kOptionModifierMapForMetaRight), kKeyHangul);
}

QTEST_MAIN(ServerConfigTests)
