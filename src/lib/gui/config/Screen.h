/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ScreenConfig.h"

#include "common/QSettingsProxy.h"

#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QSettings;
class QTextStream;
class ScreenSettingsDialog;

class Screen : public ScreenConfig
{
  friend class ScreenSettingsDialog;
  friend class ScreenSetupModel;
  friend class ScreenSetupView;

  friend QDataStream &operator<<(QDataStream &outStream, const Screen &screen)
  {
    return outStream << screen.name() << screen.switchCornerSize() << screen.aliases() << screen.modifiers()
                     << screen.switchCorners() << screen.fixes() << screen.isServer();
  }

  friend QDataStream &operator>>(QDataStream &inStream, Screen &screen)
  {
    return inStream >> screen.m_Name >> screen.m_SwitchCornerSize >> screen.m_Aliases >> screen.m_Modifiers >>
           screen.m_SwitchCorners >> screen.m_Fixes >> screen.m_isServer;
  }

public:
  explicit Screen(const QString &name = QString());

  [[nodiscard]] const QPixmap &pixmap() const
  {
    return m_Pixmap;
  }
  [[nodiscard]] const QString &name() const
  {
    return m_Name;
  }
  [[nodiscard]] const QStringList &aliases() const
  {
    return m_Aliases;
  }

  [[nodiscard]] bool isNull() const
  {
    return m_Name.isEmpty();
  }
  [[nodiscard]] int modifier(int m) const
  {
    const auto legacyMeta = static_cast<int>(ScreenConfig::Modifier::Meta);
    const auto legacySuper = static_cast<int>(ScreenConfig::Modifier::Super);
    const auto metaLeft = static_cast<int>(ScreenConfig::Modifier::MetaLeft);
    const auto metaRight = static_cast<int>(ScreenConfig::Modifier::MetaRight);
    const auto superLeft = static_cast<int>(ScreenConfig::Modifier::SuperLeft);
    const auto superRight = static_cast<int>(ScreenConfig::Modifier::SuperRight);
    const auto defaultMod = static_cast<int>(ScreenConfig::Modifier::DefaultMod);

    if (m >= m_Modifiers.size() || m_Modifiers[m] == defaultMod) {
      if ((m == metaLeft || m == metaRight) && m_Modifiers.size() > legacyMeta &&
          m_Modifiers[legacyMeta] != defaultMod) {
        return m_Modifiers[legacyMeta];
      }
      if ((m == superLeft || m == superRight) && m_Modifiers.size() > legacySuper &&
          m_Modifiers[legacySuper] != defaultMod) {
        return m_Modifiers[legacySuper];
      }
      return ScreenConfig::defaultModifier(m);
    }
    return m_Modifiers[m];
  }
  [[nodiscard]] const QList<int> &modifiers() const
  {
    return m_Modifiers;
  }
  [[nodiscard]] bool switchCorner(int c) const
  {
    return m_SwitchCorners[c];
  }
  [[nodiscard]] const QList<bool> &switchCorners() const
  {
    return m_SwitchCorners;
  }
  [[nodiscard]] int switchCornerSize() const
  {
    return m_SwitchCornerSize;
  }
  [[nodiscard]] bool fix(const Fix f) const
  {
    return m_Fixes[static_cast<int8_t>(f)];
  }
  [[nodiscard]] const QList<bool> &fixes() const
  {
    return m_Fixes;
  }

  void loadSettings(QSettingsProxy &settings);
  void saveSettings(QSettingsProxy &settings) const;
  [[nodiscard]] QString screensSection() const;
  [[nodiscard]] QString aliasesSection() const;

  [[nodiscard]] bool swapped() const
  {
    return m_Swapped;
  }

  void setName(const QString &name)
  {
    m_Name = name;
  }
  [[nodiscard]] bool isServer() const
  {
    return m_isServer;
  }
  void markAsServer()
  {
    m_isServer = true;
  }

  bool operator==(const Screen &screen) const;

protected:
  QStringList &aliases()
  {
    return m_Aliases;
  }
  void setModifier(const Modifier m, const int n)
  {
    const auto index = static_cast<int>(m);
    if (index < 0) {
      return;
    }
    if (index >= m_Modifiers.size()) {
      const auto oldSize = m_Modifiers.size();
      m_Modifiers.resize(index + 1);
      for (auto i = oldSize; i < m_Modifiers.size(); ++i) {
        m_Modifiers[i] = static_cast<int>(ScreenConfig::Modifier::DefaultMod);
      }
    }
    m_Modifiers[index] = n;
  }
  QList<int> &modifiers()
  {
    return m_Modifiers;
  }
  void addAlias(const QString &alias)
  {
    m_Aliases.append(alias);
  }
  void setSwitchCorner(const SwitchCorner c, const bool on)
  {
    m_SwitchCorners[static_cast<int8_t>(c)] = on;
  }
  QList<bool> &switchCorners()
  {
    return m_SwitchCorners;
  }
  void setSwitchCornerSize(const int val)
  {
    m_SwitchCornerSize = val;
  }
  void setFix(const Fix f, const bool on)
  {
    m_Fixes[static_cast<int8_t>(f)] = on;
  }
  QList<bool> &fixes()
  {
    return m_Fixes;
  }
  void setSwapped(const bool on)
  {
    m_Swapped = on;
  }

private:
  QPixmap m_Pixmap = QIcon::fromTheme("video-display").pixmap(QSize(96, 96));
  QString m_Name = {};
  QStringList m_Aliases = {};
  QList<int> m_Modifiers = {0, 1, 2, 3, 4, 5, -1, -1, -1, -1};
  QList<bool> m_SwitchCorners = {false, false, false, false};
  int m_SwitchCornerSize = 0;
  QList<bool> m_Fixes{false, false, false, false};
  bool m_Swapped = false;
  bool m_isServer = false;
};
