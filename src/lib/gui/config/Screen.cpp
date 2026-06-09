/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Screen.h"
#include "config/ScreenConfig.h"

using enum ScreenConfig::Modifier;
using enum ScreenConfig::SwitchCorner;
using enum ScreenConfig::Fix;

Screen::Screen(const QString &name)
{
  setName(name);
}

void Screen::loadSettings(QSettingsProxy &settings)
{
  setName(settings.value("name").toString());

  if (name().isEmpty())
    return;

  setSwitchCornerSize(settings.value("switchCornerSize").toInt());

  readSettings(settings, aliases(), "alias", QString(""));
  readSettings(settings, modifiers(), "modifier", static_cast<int>(DefaultMod), static_cast<int>(NumModifiers));
  readSettings(settings, switchCorners(), "switchCorner", false, static_cast<int>(NumSwitchCorners));
  readSettings(settings, fixes(), "fix", 0, static_cast<int>(NumFixes));
}

void Screen::saveSettings(QSettingsProxy &settings) const
{
  settings.setValue("name", name());

  if (name().isEmpty())
    return;

  settings.setValue("switchCornerSize", switchCornerSize());

  writeSettings(settings, aliases(), "alias");
  writeSettings(settings, modifiers(), "modifier");
  writeSettings(settings, switchCorners(), "switchCorner");
  writeSettings(settings, fixes(), "fix");
}

QString Screen::screensSection() const
{
  const auto lineTemplate = QStringLiteral("\t\t%1 = %2\n");
  const int modifierOptions[] = {static_cast<int>(Shift), static_cast<int>(Ctrl), static_cast<int>(Alt)};

  QString out = QStringLiteral("\t%1:\n").arg(name());
  for (const auto option : modifierOptions) {
    if (modifier(option) != defaultModifier(option))
      out.append(lineTemplate.arg(modifierConfigName(option), modifierName(modifier(option))));
  }

  const auto appendSidedModifier = [this, &out, &lineTemplate](int common, int left, int right) {
    if (modifier(left) == modifier(right)) {
      if (modifier(left) != defaultModifier(left))
        out.append(lineTemplate.arg(modifierConfigName(common), modifierName(modifier(left))));
    } else {
      if (modifier(left) != defaultModifier(left))
        out.append(lineTemplate.arg(modifierConfigName(left), modifierName(modifier(left))));
      if (modifier(right) != defaultModifier(right))
        out.append(lineTemplate.arg(modifierConfigName(right), modifierName(modifier(right))));
    }
  };
  appendSidedModifier(static_cast<int>(Meta), static_cast<int>(MetaLeft), static_cast<int>(MetaRight));
  appendSidedModifier(static_cast<int>(Super), static_cast<int>(SuperLeft), static_cast<int>(SuperRight));

  for (int i = 0; i < fixes().size(); i++)
    out.append(lineTemplate.arg(fixName(i), fixes().at(i) ? QStringLiteral("true") : QStringLiteral("false")));

  auto corners = QStringLiteral("none");
  for (int i = 0; i < switchCorners().size(); i++) {
    if (switchCorners()[i])
      corners.append(QStringLiteral(" +%1 ").arg(switchCornerName(i)));
  }
  out.append(lineTemplate.arg(QStringLiteral("switchCorners"), corners));

  out.append(lineTemplate.arg(QStringLiteral("switchCornerSize"), QString::number(switchCornerSize())));

  return out;
}

QString Screen::aliasesSection() const
{
  QString out;
  if (!aliases().isEmpty()) {
    out = QStringLiteral("\t%1:\n").arg(name());

    for (const QString &alias : aliases())
      out.append(QStringLiteral("\t\t%1\n").arg(alias));
  }
  return out;
}

bool Screen::operator==(const Screen &screen) const
{
  return m_Name == screen.m_Name && m_Aliases == screen.m_Aliases && m_Modifiers == screen.m_Modifiers &&
         m_SwitchCorners == screen.m_SwitchCorners && m_SwitchCornerSize == screen.m_SwitchCornerSize &&
         m_Fixes == screen.m_Fixes && m_Swapped == screen.m_Swapped && m_isServer == screen.m_isServer;
}
