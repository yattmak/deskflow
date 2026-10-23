/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class SecureInputStateTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void appNames_data();
  void appNames();
  void ignoresInvalidRecords_data();
  void ignoresInvalidRecords();
  void transitions();
  void reset();
  void heartbeatRecovery();
  void recoveryWithoutInfoLog();
  void fileLogRoundTripAtWarn();
  void processGate_data();
  void processGate();
  void initialRecordBeforeListening();
};
