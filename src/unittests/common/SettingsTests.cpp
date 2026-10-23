/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsTests.h"

#include "common/KeyboardModifier.h"

#include <QFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>

void SettingsTests::initTestCase()
{
  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();
}

void SettingsTests::setSettingsFile()
{
  Settings::setSettingsFile(m_settingsFile);
}

void SettingsTests::setStateFile()
{
  Settings::setStateFile(m_stateFile);
}

void SettingsTests::settingsFile()
{
  QVERIFY(Settings::settingsFile().endsWith(m_settingsFile));
}

void SettingsTests::settingsPath()
{
  QVERIFY(Settings::settingsPath().endsWith(m_settingsPath));
}

void SettingsTests::tlsDir()
{
  QVERIFY(Settings::tlsDir().endsWith(m_expectedTlsDir));
}

void SettingsTests::tlsTrustedServersDb()
{
  QVERIFY(Settings::tlsTrustedServersDb().endsWith(m_expectedTlsServerDB));
}

void SettingsTests::tlsTrustedClientsDb()
{
  QVERIFY(Settings::tlsTrustedClientsDb().endsWith(m_expectedTlsClientDB));
}

void SettingsTests::checkValidSettings()
{
  QSignalSpy spy(Settings::instance(), &Settings::settingsChanged);
  QVERIFY(spy.isValid());

  const auto keysToCheck = QRegularExpression(QLatin1String("[^%1]").arg(Settings::Core::ComputerName));
  const auto validKeys = Settings::validKeys().filter(keysToCheck);
  for (const auto &setting : validKeys) {
    const auto value = Settings::value(setting).toString();
    QCOMPARE(Settings::defaultValue(setting).toString(), value);

    Settings::setValue(setting, "NEW_VALUE");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.first().at(0)), setting);
    QCOMPARE(Settings::value(setting).toString(), "NEW_VALUE");

    Settings::setValue(setting);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(Settings::value(setting).toString(), value);

    // Reset the spy for the next loop
    spy.clear();
    QCOMPARE(spy.count(), 0);
  }
}

void SettingsTests::checkCleanComputerName()
{
  const auto input = QStringLiteral("--!_ _-C@o#mpu$t%e^&*(r)= +Name\n[1]2|3?4--5>6<,7`~/8*90\\.lan--..    ..");
  const auto expected = QStringLiteral("Computer_Name_1234--567890.lan");

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::checkCleanComputerName_LongName()
{
  QString input;
  input.fill('f', 300);
  input.prepend('.');

  QString expected;
  expected.fill('f', 255);

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::checkClearState()
{
  Settings::setValue(Settings::Gui::WindowGeometry, QRect(1, 2, 3, 4));
  QVERIFY(Settings::value(Settings::Gui::WindowGeometry).toRect().isValid());

  Settings::clearState();

  QVERIFY(!Settings::value(Settings::Gui::WindowGeometry).toRect().isValid());
}

void SettingsTests::modifierProtocolValues()
{
  QCOMPARE(static_cast<int>(KeyboardModifier::AltGr), 5);
  QCOMPARE(static_cast<int>(KeyboardModifier::None), 6);
  QCOMPARE(modifierIDValueFromString("altgr"), kKeyModifierIDAltGr);
  QCOMPARE(modifierIDValueFromString("none"), kKeyModifierIDNull);
  QCOMPARE(modifierIDValueFromString("meta"), kKeyModifierIDMeta);
  for (const auto &name : {"hangul", "HAEN", "kc_haen"}) {
    QCOMPARE(modifierValueFromString(name), static_cast<int>(KeyboardModifier::Hangul));
    QCOMPARE(modifierIDValueFromString(name), kKeyHangul);
  }
  for (const auto &name : {"hanja", "Hangul_Hanja", "HAEJ", "kc_haej"})
    QCOMPARE(modifierIDValueFromString(name), kKeyHanja);
  QVERIFY(modifierIDValueFromString("hangul") != static_cast<int>(KeyboardModifier::Hangul));
  QCOMPARE(modifierIDValueFromString("invalid"), -1);
}

void SettingsTests::migrateModifierArrays_data()
{
  QTest::addColumn<QList<int>>("modifiers");
  QTest::addColumn<QStringList>("expected");
  QTest::newRow("custom-none-hangul-sides")
      << QList<int>{5, 6, 2, 1, 4, 5, -1, 6, 5, -1}
      << QStringList{"none", "hangul", "alt", "ctrl", "super", "altgr", "", "hangul", "none", ""};
  QTest::newRow("custom-default-inheritance")
      << QList<int>{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
      << QStringList{"shift", "ctrl", "alt", "meta", "super", "altgr", "", "", "", ""};
  QTest::newRow("custom-explicit-sides") << QList<int>{0, 1, 2, 6, 5, 5, 3, 4, 0, 1}
                                         << QStringList{"shift", "ctrl", "alt",   "hangul", "none",
                                                        "altgr", "meta", "super", "shift",  "ctrl"};
  QTest::newRow("six-slot-upstream-semantics")
      << QList<int>{5, 6, -1, 3, 4, -1}
      << QStringList{"altgr", "none", "alt", "meta", "super", "altgr", "", "", "", ""};
  QTest::newRow("seven-slot-upstream-semantics")
      << QList<int>{6, 5, -1, -1, -1, -1, 6}
      << QStringList{"none", "altgr", "alt", "meta", "super", "altgr", "", "", "", ""};
  QTest::newRow("missing-modifier-array")
      << QList<int>{} << QStringList{"shift", "ctrl", "alt", "meta", "super", "altgr", "", "", "", ""};
}

void SettingsTests::migrateModifierArrays()
{
  QFETCH(QList<int>, modifiers);
  QFETCH(QStringList, expected);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto file = directory.filePath("legacy.conf");
  const auto previousFile = Settings::settingsFile();
  const auto restore = qScopeGuard([previousFile] { Settings::setSettingsFile(previousFile); });
  {
    QSettings legacy(file, QSettings::IniFormat);
    legacy.beginGroup("internalConfig");
    legacy.beginWriteArray("screens");
    legacy.setArrayIndex(0); // the first screen must not be skipped
    legacy.setValue("name", "legacy");
    if (!modifiers.isEmpty()) {
      legacy.beginWriteArray("modifierArray");
      for (int i = 0; i < modifiers.size(); i++) {
        legacy.setArrayIndex(i);
        legacy.setValue("modifier", modifiers.at(i));
      }
      legacy.endArray();
    }
    legacy.endArray();
    legacy.endGroup();
    legacy.sync();
  }
  const QStringList keys = {Settings::Computer::ModifierShift,     Settings::Computer::ModifierCtrl,
                            Settings::Computer::ModifierAlt,       Settings::Computer::ModifierMeta,
                            Settings::Computer::ModifierSuper,     Settings::Computer::ModifierAltGr,
                            Settings::Computer::ModifierMetaLeft,  Settings::Computer::ModifierMetaRight,
                            Settings::Computer::ModifierSuperLeft, Settings::Computer::ModifierSuperRight};
  Settings::setSettingsFile(file);
  QCOMPARE(Settings::knownComputers(), QStringList{"legacy"});
  const auto reloadSettings = [&] {
    Settings::save();
    Settings::setSettingsFile(directory.filePath("reload.conf"));
    Settings::setSettingsFile(file);
  };
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < keys.size(); i++)
      QCOMPARE(Settings::value(keys.at(i).arg("legacy")).toString(), expected.at(i));
    reloadSettings();
  }
  // An inherited side must remain unset, not become a frozen copy of Meta.
  Settings::setValue(Settings::Computer::ModifierMeta.arg("legacy"), "hanja");
  reloadSettings();
  QCOMPARE(Settings::value(Settings::Computer::ModifierMeta.arg("legacy")).toString(), "hanja");
  QCOMPARE(Settings::value(Settings::Computer::ModifierMetaLeft.arg("legacy")).toString(), expected.at(6));
  Settings::setValue(Settings::Computer::ModifierMetaRight.arg("legacy"), "hanja");
  reloadSettings();
  QCOMPARE(Settings::value(Settings::Computer::ModifierMetaRight.arg("legacy")).toString(), "hanja");
}

void SettingsTests::migrateModifiersPreservesNewSettings()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto file = directory.filePath("mixed.conf");
  const auto previousFile = Settings::settingsFile();
  const auto restore = qScopeGuard([previousFile] { Settings::setSettingsFile(previousFile); });
  {
    QSettings legacy(file, QSettings::IniFormat);
    legacy.setValue("screen_mixed/name", "mixed");
    legacy.setValue("screen_mixed/modifierMetaLeft", "none");
    legacy.setValue("screen_mixed/modifierMetaRight", "hangul");
    legacy.setValue("screen_mixed/modifierSuperLeft", "ctrl");
    legacy.setValue("screen_mixed/modifierSuperRight", "Return");
    legacy.setValue(Settings::Computer::ModifierMetaRight.arg("mixed"), "hanja");
    legacy.sync();
  }
  const QStringList keys = {
      Settings::Computer::ModifierMetaLeft, Settings::Computer::ModifierMetaRight,
      Settings::Computer::ModifierSuperLeft, Settings::Computer::ModifierSuperRight
  };
  const QStringList expected = {"none", "hanja", "ctrl", "Return"};
  Settings::setSettingsFile(file);
  for (int pass = 0; pass < 2; pass++) {
    Settings::save();
    {
      QSettings cleaned(file, QSettings::IniFormat);
      for (int i = 0; i < keys.size(); i++) {
        const auto key = keys.at(i).arg("mixed");
        QVERIFY(cleaned.contains(key)); // cleanSettings must retain every computer side key
        QCOMPARE(cleaned.value(key).toString(), expected.at(i));
        QCOMPARE(Settings::value(key).toString(), expected.at(i));
      }
      QVERIFY(cleaned.allKeys().filter("screen_").isEmpty());
    }
    Settings::setSettingsFile(directory.filePath("reload.conf"));
    Settings::setSettingsFile(file);
  }
}

QTEST_MAIN(SettingsTests)
