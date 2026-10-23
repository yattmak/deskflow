/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"

#include <QString>

namespace deskflow::gui {

class SecureInputState
{
public:
  static bool acceptsUpdates(deskflow::core::ProcessState process, deskflow::core::ConnectionState connection);

  bool updateFromLogLine(const QString &line);
  bool reset();

  bool isActive() const
  {
    return m_active;
  }
  const QString &app() const
  {
    return m_app;
  }

private:
  bool setState(bool active, const QString &app);

  bool m_active = false;
  QString m_app;
};

} // namespace deskflow::gui
