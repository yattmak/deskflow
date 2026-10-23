/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerTests.h"

#include "common/Settings.h"
#include "gui/config/Computer.h"
#include "gui/dialogs/ComputerSettingsDialog.h"

#include <QComboBox>
#include <QDataStream>
#include <QTemporaryDir>

namespace {
class EditableComputer : public Computer
{
public:
  using Computer::Computer;
  using Computer::setModifier;
  using Computer::setSideModifier;
};
} // namespace

void ComputerTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void ComputerTests::basicFunctionality()
{
  Computer computer;
  QVERIFY(computer.isNull());

  computer.setName("stub");
  QVERIFY(!computer.isNull());

  computer.saveSettings(Settings::proxy());
  computer.loadSettings(Settings::proxy());
  QCOMPARE("stub", computer.name());
}

void ComputerTests::sideModifierInheritance()
{
  EditableComputer computer("inheritance");
  using enum KeyboardModifierSide;
  for (const auto side : {MetaLeft, MetaRight, SuperLeft, SuperRight})
    QCOMPARE(computer.sideModifierOverride(side), -1);
  QCOMPARE(computer.sideModifier(MetaLeft), static_cast<int>(KeyboardModifier::Meta));
  QCOMPARE(computer.sideModifier(SuperRight), static_cast<int>(KeyboardModifier::Super));

  computer.setModifier(KeyboardModifier::Meta, static_cast<int>(KeyboardModifier::Ctrl));
  computer.setModifier(KeyboardModifier::Super, static_cast<int>(KeyboardModifier::None));
  QCOMPARE(computer.sideModifier(MetaLeft), static_cast<int>(KeyboardModifier::Ctrl));
  QCOMPARE(computer.sideModifier(MetaRight), static_cast<int>(KeyboardModifier::Ctrl));
  QCOMPARE(computer.sideModifier(SuperLeft), static_cast<int>(KeyboardModifier::None));
  computer.setSideModifier(MetaRight, static_cast<int>(KeyboardModifier::Hangul));
  QCOMPARE(computer.sideModifier(MetaRight), static_cast<int>(KeyboardModifier::Hangul));
  computer.setModifier(KeyboardModifier::Meta, static_cast<int>(KeyboardModifier::Hanja));
  QCOMPARE(computer.sideModifier(MetaLeft), static_cast<int>(KeyboardModifier::Hanja));
  QCOMPARE(computer.sideModifier(MetaRight), static_cast<int>(KeyboardModifier::Hangul));
  computer.setSideModifier(MetaRight, -1);
  QCOMPARE(computer.sideModifier(MetaRight), static_cast<int>(KeyboardModifier::Hanja));
  computer.setModifier(KeyboardModifier::Meta, -1);
  QCOMPARE(computer.sideModifier(MetaRight), static_cast<int>(KeyboardModifier::Meta));
}

void ComputerTests::modifierSettingsRoundTrip()
{
  EditableComputer computer("modifier-roundtrip");
  computer.setModifier(KeyboardModifier::Meta, static_cast<int>(KeyboardModifier::Ctrl));
  computer.setSideModifier(KeyboardModifierSide::MetaRight, static_cast<int>(KeyboardModifier::Hangul));
  computer.setSideModifier(KeyboardModifierSide::SuperLeft, static_cast<int>(KeyboardModifier::None));
  computer.setSideModifier(KeyboardModifierSide::SuperRight, static_cast<int>(KeyboardModifier::Hanja));
  computer.saveSettings(Settings::proxy());
  Settings::save();
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  Settings::setSettingsFile(directory.filePath("reload.conf"));
  Settings::setSettingsFile(m_settingsFile);

  Computer restored;
  restored.loadSettings(Settings::proxy());
  QCOMPARE(restored, computer);
  QCOMPARE(restored.sideModifierOverride(KeyboardModifierSide::MetaLeft), -1);
  QCOMPARE(restored.sideModifier(KeyboardModifierSide::MetaLeft), static_cast<int>(KeyboardModifier::Ctrl));
  QCOMPARE(restored.modifier(static_cast<int>(KeyboardModifier::AltGr)), static_cast<int>(KeyboardModifier::AltGr));
  QCOMPARE(Settings::value(Settings::Computer::ModifierMetaRight.arg(computer.name())).toString(), "hangul");
  QCOMPARE(Settings::value(Settings::Computer::ModifierSuperLeft.arg(computer.name())).toString(), "none");
  QCOMPARE(Settings::value(Settings::Computer::ModifierSuperRight.arg(computer.name())).toString(), "hanja");

  QCOMPARE(Computer::modifierTargetFromOption("Return"), kKeyReturn);
  QCOMPARE(Computer::modifierTargetFromOption(Computer::modifierTargetToOption(kKeyReturn)), kKeyReturn);
  for (const auto *target : {"CapsLock", "NumLock", "ScrollLock", "\\uEFE5", "\\uEF7F", "\\uEF14"})
    QCOMPARE(Computer::modifierTargetFromOption(target), static_cast<int>(KeyboardModifier::DefaultMod));
  Settings::setValue(Settings::Computer::ModifierMetaRight.arg(computer.name()), "Return");
  restored.loadSettings(Settings::proxy());
  QCOMPARE(restored.sideModifier(KeyboardModifierSide::MetaRight), kKeyReturn);
  restored.saveSettings(Settings::proxy());
  restored.loadSettings(Settings::proxy());
  QCOMPARE(restored.sideModifier(KeyboardModifierSide::MetaRight), kKeyReturn);
}

void ComputerTests::modifierDataStreamRoundTrip()
{
  EditableComputer computer("serialized-modifiers");
  computer.setModifier(KeyboardModifier::Meta, static_cast<int>(KeyboardModifier::Ctrl));
  computer.setSideModifier(KeyboardModifierSide::MetaRight, static_cast<int>(KeyboardModifier::Hangul));
  QByteArray bytes;
  {
    QDataStream output(&bytes, QIODevice::WriteOnly);
    output << computer;
    QCOMPARE(output.status(), QDataStream::Ok);
  }
  Computer restored;
  QDataStream input(bytes);
  input >> restored;
  QCOMPARE(input.status(), QDataStream::Ok);
  QCOMPARE(restored, computer);
  QCOMPARE(restored.sideModifierOverride(KeyboardModifierSide::MetaLeft), -1);
}

void ComputerTests::modifierDialogTargets()
{
  EditableComputer computer("dialog-modifiers");
  computer.setModifier(KeyboardModifier::Meta, static_cast<int>(KeyboardModifier::Ctrl));
  computer.setSideModifier(KeyboardModifierSide::MetaRight, kKeyReturn);
  ComputerSettingsDialog dialog(nullptr, &computer);
  auto *metaLeft = dialog.findChild<QComboBox *>("comboMetaLeft");
  auto *metaRight = dialog.findChild<QComboBox *>("comboMetaRight");
  auto *superRight = dialog.findChild<QComboBox *>("comboSuperRight");
  auto *altGr = dialog.findChild<QComboBox *>("comboAltGr");
  QVERIFY(metaLeft);
  QVERIFY(metaRight);
  QVERIFY(superRight);
  QVERIFY(altGr);
  QCOMPARE(metaLeft->currentData().toInt(), -1);
  QCOMPARE(metaRight->currentData().toInt(), kKeyReturn);
  QCOMPARE(altGr->currentData().toInt(), static_cast<int>(KeyboardModifier::AltGr));
  const auto hangul = metaRight->findData(static_cast<int>(KeyboardModifier::Hangul));
  const auto hanja = superRight->findData(static_cast<int>(KeyboardModifier::Hanja));
  QVERIFY(hangul >= 0);
  QVERIFY(hanja >= 0);
  metaRight->setCurrentIndex(hangul);
  superRight->setCurrentIndex(hanja);
  dialog.accept();
  QCOMPARE(dialog.result(), QDialog::Accepted);
  QCOMPARE(computer.sideModifierOverride(KeyboardModifierSide::MetaLeft), -1);
  QCOMPARE(computer.sideModifier(KeyboardModifierSide::MetaLeft), static_cast<int>(KeyboardModifier::Ctrl));
  QCOMPARE(computer.sideModifier(KeyboardModifierSide::MetaRight), static_cast<int>(KeyboardModifier::Hangul));
  QCOMPARE(computer.sideModifier(KeyboardModifierSide::SuperRight), static_cast<int>(KeyboardModifier::Hanja));
}

QTEST_MAIN(ComputerTests)
