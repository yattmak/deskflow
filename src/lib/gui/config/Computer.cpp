/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Computer.h"
#include "config/ComputerConfig.h"
#include "deskflow/KeyMap.h"
#include <common/Settings.h>

using enum KeyboardModifier;
using enum ComputerConfig::SwitchCorner;
using enum ComputerConfig::Fix;

Computer::Computer(const QString &name)
{
  setName(name);
}

int Computer::modifierTargetFromOption(const QString &option)
{
  const auto modifier = keyboardModifierFromString(option);
  if (modifier != DefaultMod)
    return static_cast<int>(modifier);
  KeyID key = kKeyNone;
  if (deskflow::KeyMap::parseKey(option.toStdString(), key) && key > static_cast<KeyID>(Hanja) &&
      !isUnsupportedModifierKeyTarget(key))
    return static_cast<int>(key);
  return static_cast<int>(DefaultMod);
}

QString Computer::modifierTargetToOption(int target)
{
  if (target >= static_cast<int>(DefaultMod) && target <= static_cast<int>(Hanja))
    return valueToKeyboardModifierOption(target);
  return QString::fromStdString(deskflow::KeyMap::formatKey(static_cast<KeyID>(target), 0));
}

void Computer::loadSettings(QSettingsProxy &settings)
{
  const auto name = settings.value("name").toString();
  setName(name);

  if (name.isEmpty())
    return;

  setModifier(Alt, modifierTargetFromOption(Settings::value(Settings::Computer::ModifierAlt.arg(name)).toString()));
  setModifier(AltGr, modifierTargetFromOption(Settings::value(Settings::Computer::ModifierAltGr.arg(name)).toString()));
  setModifier(Ctrl, modifierTargetFromOption(Settings::value(Settings::Computer::ModifierCtrl.arg(name)).toString()));
  setModifier(Meta, modifierTargetFromOption(Settings::value(Settings::Computer::ModifierMeta.arg(name)).toString()));
  setModifier(Shift, modifierTargetFromOption(Settings::value(Settings::Computer::ModifierShift.arg(name)).toString()));
  setModifier(Super, modifierTargetFromOption(Settings::value(Settings::Computer::ModifierSuper.arg(name)).toString()));

  setSideModifier(
      KeyboardModifierSide::MetaLeft,
      modifierTargetFromOption(Settings::value(Settings::Computer::ModifierMetaLeft.arg(name)).toString())
  );
  setSideModifier(
      KeyboardModifierSide::MetaRight,
      modifierTargetFromOption(Settings::value(Settings::Computer::ModifierMetaRight.arg(name)).toString())
  );
  setSideModifier(
      KeyboardModifierSide::SuperLeft,
      modifierTargetFromOption(Settings::value(Settings::Computer::ModifierSuperLeft.arg(name)).toString())
  );
  setSideModifier(
      KeyboardModifierSide::SuperRight,
      modifierTargetFromOption(Settings::value(Settings::Computer::ModifierSuperRight.arg(name)).toString())
  );

  setSwitchCornerSize(Settings::value(Settings::Computer::SwitchCornerSize.arg(name)).toInt());

  m_SwitchCorners[static_cast<int>(TopLeft)] =
      Settings::value(Settings::Computer::SwitchCornerTopLeft.arg(name)).toBool();
  m_SwitchCorners[static_cast<int>(TopRight)] =
      Settings::value(Settings::Computer::SwitchCornerTopRight.arg(name)).toBool();
  m_SwitchCorners[static_cast<int>(BottomLeft)] =
      Settings::value(Settings::Computer::SwitchCornerBottomLeft.arg(name)).toBool();
  m_SwitchCorners[static_cast<int>(BottomRight)] =
      Settings::value(Settings::Computer::SwitchCornerBottomRight.arg(name)).toBool();

  m_Fixes[static_cast<int>(CapsLock)] = Settings::value(Settings::Computer::HalfDuplexCapsLock.arg(name)).toBool();
  m_Fixes[static_cast<int>(NumLock)] = Settings::value(Settings::Computer::HalfDuplexNumLock.arg(name)).toBool();
  m_Fixes[static_cast<int>(ScrollLock)] = Settings::value(Settings::Computer::HalfDuplexScrollLock.arg(name)).toBool();
  m_Fixes[static_cast<int>(XTest)] = Settings::value(Settings::Computer::XtestIsXineramaUnaware.arg(name)).toBool();

  m_Aliases = Settings::value(Settings::Computer::Aliases.arg(name)).toStringList();
}

void Computer::saveSettings(QSettingsProxy &settings) const
{
  const auto computerName = name();
  settings.setValue("name", computerName);

  if (computerName.isEmpty())
    return;

  Settings::setValue(Settings::Computer::Name.arg(computerName), computerName);
  Settings::setValue(Settings::Computer::Aliases.arg(computerName), m_Aliases);
  Settings::setValue(Settings::Computer::HalfDuplexCapsLock.arg(computerName), m_Fixes[static_cast<int>(CapsLock)]);
  Settings::setValue(Settings::Computer::HalfDuplexNumLock.arg(computerName), m_Fixes[static_cast<int>(NumLock)]);
  Settings::setValue(Settings::Computer::HalfDuplexScrollLock.arg(computerName), m_Fixes[static_cast<int>(ScrollLock)]);
  Settings::setValue(Settings::Computer::XtestIsXineramaUnaware.arg(computerName), m_Fixes[static_cast<int>(XTest)]);
  Settings::setValue(Settings::Computer::SwitchCornerSize.arg(computerName), switchCornerSize());
  Settings::setValue(
      Settings::Computer::SwitchCornerTopLeft.arg(computerName), m_SwitchCorners[static_cast<int>(TopLeft)]
  );
  Settings::setValue(
      Settings::Computer::SwitchCornerTopRight.arg(computerName), m_SwitchCorners[static_cast<int>(TopRight)]
  );
  Settings::setValue(
      Settings::Computer::SwitchCornerBottomLeft.arg(computerName), m_SwitchCorners[static_cast<int>(BottomLeft)]
  );
  Settings::setValue(
      Settings::Computer::SwitchCornerBottomRight.arg(computerName), m_SwitchCorners[static_cast<int>(BottomRight)]
  );

  Settings::setValue(
      Settings::Computer::ModifierAlt.arg(computerName),
      modifierTargetToOption(m_Modifiers.at(static_cast<qsizetype>(KeyboardModifier::Alt)))
  );
  Settings::setValue(
      Settings::Computer::ModifierAltGr.arg(computerName),
      modifierTargetToOption(m_Modifiers.at(static_cast<qsizetype>(KeyboardModifier::AltGr)))
  );
  Settings::setValue(
      Settings::Computer::ModifierCtrl.arg(computerName),
      modifierTargetToOption(m_Modifiers.at(static_cast<qsizetype>(KeyboardModifier::Ctrl)))
  );
  Settings::setValue(
      Settings::Computer::ModifierMeta.arg(computerName),
      modifierTargetToOption(m_Modifiers.at(static_cast<qsizetype>(KeyboardModifier::Meta)))
  );
  Settings::setValue(
      Settings::Computer::ModifierShift.arg(computerName),
      modifierTargetToOption(m_Modifiers.at(static_cast<qsizetype>(KeyboardModifier::Shift)))
  );
  Settings::setValue(
      Settings::Computer::ModifierSuper.arg(computerName),
      modifierTargetToOption(m_Modifiers.at(static_cast<qsizetype>(KeyboardModifier::Super)))
  );
  Settings::setValue(
      Settings::Computer::ModifierMetaLeft.arg(computerName),
      modifierTargetToOption(sideModifierOverride(KeyboardModifierSide::MetaLeft))
  );
  Settings::setValue(
      Settings::Computer::ModifierMetaRight.arg(computerName),
      modifierTargetToOption(sideModifierOverride(KeyboardModifierSide::MetaRight))
  );
  Settings::setValue(
      Settings::Computer::ModifierSuperLeft.arg(computerName),
      modifierTargetToOption(sideModifierOverride(KeyboardModifierSide::SuperLeft))
  );
  Settings::setValue(
      Settings::Computer::ModifierSuperRight.arg(computerName),
      modifierTargetToOption(sideModifierOverride(KeyboardModifierSide::SuperRight))
  );
}

QString Computer::screensSection() const
{
  return QStringLiteral("\t%1:\n").arg(name());
}

bool Computer::operator==(const Computer &computer) const
{
  return m_Name == computer.m_Name && m_Aliases == computer.m_Aliases && m_Modifiers == computer.m_Modifiers &&
         m_SideModifiers == computer.m_SideModifiers && m_SwitchCorners == computer.m_SwitchCorners &&
         m_SwitchCornerSize == computer.m_SwitchCornerSize && m_Fixes == computer.m_Fixes &&
         m_Swapped == computer.m_Swapped && m_isServer == computer.m_isServer;
}
