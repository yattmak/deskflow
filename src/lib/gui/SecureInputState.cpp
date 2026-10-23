/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SecureInputState.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace deskflow::gui {

bool SecureInputState::acceptsUpdates(deskflow::core::ProcessState process, deskflow::core::ConnectionState connection)
{
  using deskflow::core::ConnectionState;
  using deskflow::core::ProcessState;
  // Connecting is set before launching the core, so its constructor's snapshot need not wait for Listening.
  return (process == ProcessState::Starting || process == ProcessState::Started) &&
         connection != ConnectionState::Disconnected;
}

bool SecureInputState::updateFromLogLine(const QString &line)
{
  // Log::print bypasses makeMessage for PRINT; console and file outputters preserve the unprefixed record.
  static const auto prefix = QStringLiteral("secure input status: ");
  if (!line.startsWith(prefix)) {
    return false;
  }

  const auto document = QJsonDocument::fromJson(line.mid(prefix.size()).toUtf8());
  if (!document.isObject()) {
    return false;
  }
  const auto object = document.object();
  const auto active = object.value(QStringLiteral("active"));
  const auto app = object.value(QStringLiteral("app"));
  if (!active.isBool() || !app.isString()) {
    return false;
  }
  return setState(active.toBool(), app.toString());
}

bool SecureInputState::reset()
{
  return setState(false, {});
}

bool SecureInputState::setState(bool active, const QString &app)
{
  const auto blockedApp = active ? app : QString();
  if (m_active == active && m_app == blockedApp) {
    return false;
  }
  m_active = active;
  m_app = blockedApp;
  return true;
}

} // namespace deskflow::gui
