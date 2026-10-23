/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SecureInputStateTests.h"

#include "base/Log.h"
#include "base/LogOutputters.h"
#include "gui/FileTail.h"
#include "gui/SecureInputState.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

using deskflow::gui::SecureInputState;

namespace {
QString statusLine(bool active, const QString &app = {})
{
  const QJsonObject status{{QStringLiteral("active"), active}, {QStringLiteral("app"), app}};
  return QStringLiteral("secure input status: ") +
         QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact));
}
} // namespace

void SecureInputStateTests::appNames_data()
{
  QTest::addColumn<QString>("app");
  QTest::newRow("simple") << QStringLiteral("Terminal");
  QTest::newRow("colon") << QStringLiteral("Terminal: Profile: Work");
  QTest::newRow("quotes-and-backslash") << QStringLiteral("A \"secure\" app \\ Work");
  QTest::newRow("unicode") << QString::fromUtf8("암호 입력 🔐");
  QTest::newRow("mnemonic-and-ipc-delimiter") << QStringLiteral("Passwords & Keys=Work");
  QTest::newRow("line-breaks") << QStringLiteral("App\nName\rWith\tWhitespace");
  QTest::newRow("log-like-name") << QStringLiteral("secure input status: {\"active\":false,\"app\":\"\"}");
  QTest::newRow("empty-fallback") << QString();
}

void SecureInputStateTests::appNames()
{
  QFETCH(QString, app);
  SecureInputState state;
  const auto line = statusLine(true, app);
  QVERIFY(!line.contains(QLatin1Char('\n')));
  QVERIFY(state.updateFromLogLine(line));
  QVERIFY(state.isActive());
  QCOMPARE(state.app(), app);
  QVERIFY(!state.updateFromLogLine(line));
}

void SecureInputStateTests::ignoresInvalidRecords_data()
{
  QTest::addColumn<QString>("line");
  QTest::newRow("unrelated") << QStringLiteral("WARNING: application is blocking the keyboard");
  QTest::newRow("not-a-state-record") << QStringLiteral(
      "WARNING: secure input status: {\"active\":false,\"app\":\"\"}"
  );
  QTest::newRow("timestamped-human-log") << QStringLiteral(
      "[2026-10-23T12:34:56.000] INFO: secure input status: {\"active\":false,\"app\":\"\"}"
  );
  QTest::newRow("truncated") << QStringLiteral("secure input status: {\"active\":false");
  QTest::newRow("non-object") << QStringLiteral("secure input status: []");
  QTest::newRow("missing-active") << QStringLiteral("secure input status: {\"app\":\"Terminal\"}");
  QTest::newRow("missing-app") << QStringLiteral("secure input status: {\"active\":false}");
  QTest::newRow("wrong-active-type") << QStringLiteral("secure input status: {\"active\":\"false\",\"app\":\"\"}");
  QTest::newRow("wrong-app-type") << QStringLiteral("secure input status: {\"active\":false,\"app\":null}");
  QTest::newRow("trailing-garbage") << QStringLiteral("secure input status: {\"active\":false,\"app\":\"\"} garbage");
}

void SecureInputStateTests::ignoresInvalidRecords()
{
  QFETCH(QString, line);
  SecureInputState state;
  QVERIFY(state.updateFromLogLine(statusLine(true, QStringLiteral("Terminal"))));
  QVERIFY(!state.updateFromLogLine(line));
  QVERIFY(state.isActive());
  QCOMPARE(state.app(), QStringLiteral("Terminal"));
}

void SecureInputStateTests::transitions()
{
  SecureInputState state;
  QVERIFY(!state.isActive());
  QVERIFY(state.app().isEmpty());
  QVERIFY(!state.updateFromLogLine(statusLine(false)));
  QVERIFY(state.updateFromLogLine(statusLine(true, QStringLiteral("Terminal"))));
  QVERIFY(state.updateFromLogLine(statusLine(true, QStringLiteral("Passwords"))));
  QCOMPARE(state.app(), QStringLiteral("Passwords"));
  QVERIFY(state.updateFromLogLine(statusLine(false)));
  QVERIFY(!state.isActive());
  QVERIFY(state.app().isEmpty());
  QVERIFY(!state.updateFromLogLine(statusLine(false)));
  QVERIFY(state.updateFromLogLine(statusLine(true, QStringLiteral("Terminal"))));
}

void SecureInputStateTests::reset()
{
  SecureInputState state;
  QVERIFY(!state.reset());
  QVERIFY(state.updateFromLogLine(statusLine(true, QStringLiteral("Terminal"))));
  QVERIFY(state.reset());
  QVERIFY(!state.isActive());
  QVERIFY(state.app().isEmpty());
  QVERIFY(!state.reset());
  QVERIFY(state.updateFromLogLine(statusLine(true, QStringLiteral("Terminal"))));
}

void SecureInputStateTests::heartbeatRecovery()
{
  SecureInputState state;
  const auto active = statusLine(true, QStringLiteral("Terminal"));
  const auto inactive = statusLine(false);

  // A failed initial record is followed by the same snapshot on the next poll.
  QVERIFY(!state.updateFromLogLine(QStringLiteral("secure input status: {\"active\":true")));
  QVERIFY(state.updateFromLogLine(active));
  QVERIFY(state.isActive());
  QVERIFY(!state.updateFromLogLine(active));

  // Missing the recovery must not leave a warning once a complete inactive heartbeat arrives.
  QVERIFY(!state.updateFromLogLine(QStringLiteral("secure input status: {\"active\":false")));
  QVERIFY(state.isActive());
  QVERIFY(state.updateFromLogLine(inactive));
  QVERIFY(!state.isActive());
  QVERIFY(state.app().isEmpty());
  QVERIFY(!state.updateFromLogLine(inactive));

  // A reconnect clears the UI, but the unchanged active state can populate it again.
  QVERIFY(state.updateFromLogLine(active));
  QVERIFY(state.reset());
  QVERIFY(state.updateFromLogLine(active));
  QVERIFY(!state.updateFromLogLine(active));
}

void SecureInputStateTests::recoveryWithoutInfoLog()
{
  SecureInputState state;
  // WARN filtering removes the human-readable INFO recovery, but not either LOG_PRINT state record.
  QVERIFY(!state.updateFromLogLine(
      QStringLiteral("WARNING: secure input active: application \"Terminal\" is blocking keyboard capture")
  ));
  QVERIFY(state.updateFromLogLine(statusLine(true, QStringLiteral("Terminal"))));
  QVERIFY(state.updateFromLogLine(statusLine(false, QStringLiteral("stale app is discarded"))));
  QVERIFY(!state.isActive());
  QVERIFY(state.app().isEmpty());
}

void SecureInputStateTests::fileLogRoundTripAtWarn()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("secure-input.log"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();

  deskflow::gui::FileTail tail(path);
  QSignalSpy lines(&tail, &deskflow::gui::FileTail::newLine);
  QVERIFY(lines.isValid());

  Log log(false);
  log.pop_front();
  log.setFilter(LogLevel::Level::Warning);
  log.insert(new FileLogOutputter(path));

  const auto app = QString::fromUtf8("암호: \"Work\" & Keys=1\nProfile");
  const auto active = statusLine(true, app);
  const auto inactive = statusLine(false);
  // Supply a filename and line explicitly: PRINT must omit both, as well as timestamps and severity.
  log.print("Server.cpp", 310, CLOG_TAG_PRINT "%s", qPrintable(active));
  QVERIFY(QMetaObject::invokeMethod(&tail, "handleFileChanged", Qt::DirectConnection, Q_ARG(QString, path)));
  QCOMPARE(lines.count(), 1);
  QCOMPARE(lines.at(0).at(0).toString(), active);

  SecureInputState state;
  QVERIFY(state.updateFromLogLine(lines.at(0).at(0).toString()));
  QCOMPARE(state.app(), app);

  log.print("Server.cpp", 293, CLOG_TAG_INFO "secure input inactive: keyboard capture restored");
  log.print("Server.cpp", 310, CLOG_TAG_PRINT "%s", qPrintable(inactive));
  QVERIFY(QMetaObject::invokeMethod(&tail, "handleFileChanged", Qt::DirectConnection, Q_ARG(QString, path)));
  QCOMPARE(lines.count(), 2);
  QCOMPARE(lines.at(1).at(0).toString(), inactive);
  QVERIFY(state.updateFromLogLine(lines.at(1).at(0).toString()));
  QVERIFY(!state.isActive());
  QVERIFY(state.app().isEmpty());
}

void SecureInputStateTests::processGate_data()
{
  using deskflow::core::ConnectionState;
  using deskflow::core::ProcessState;
  QTest::addColumn<ProcessState>("process");
  QTest::addColumn<ConnectionState>("connection");
  QTest::addColumn<bool>("accept");

  QTest::newRow("starting-before-launch") << ProcessState::Starting << ConnectionState::Connecting << true;
  QTest::newRow("started-before-listening") << ProcessState::Started << ConnectionState::Connecting << true;
  QTest::newRow("listening-with-no-clients") << ProcessState::Started << ConnectionState::Listening << true;
  QTest::newRow("connected") << ProcessState::Started << ConnectionState::Connected << true;
  QTest::newRow("starting-disconnected") << ProcessState::Starting << ConnectionState::Disconnected << false;
  QTest::newRow("started-disconnected") << ProcessState::Started << ConnectionState::Disconnected << false;
  QTest::newRow("stopping") << ProcessState::Stopping << ConnectionState::Connected << false;
  QTest::newRow("stopped") << ProcessState::Stopped << ConnectionState::Listening << false;
  QTest::newRow("retry-pending") << ProcessState::RetryPending << ConnectionState::Connecting << false;
}

void SecureInputStateTests::processGate()
{
  QFETCH(deskflow::core::ProcessState, process);
  QFETCH(deskflow::core::ConnectionState, connection);
  QFETCH(bool, accept);
  QCOMPARE(SecureInputState::acceptsUpdates(process, connection), accept);
}

void SecureInputStateTests::initialRecordBeforeListening()
{
  using deskflow::core::ConnectionState;
  using deskflow::core::ProcessState;
  SecureInputState state;
  const auto active = statusLine(true, QStringLiteral("Terminal"));

  // CoreProcess::start sets Connecting before launching the child; Server's constructor emits the snapshot.
  QVERIFY(SecureInputState::acceptsUpdates(ProcessState::Starting, ConnectionState::Connecting));
  QVERIFY(state.updateFromLogLine(active));
  QVERIFY(state.isActive());
  QCOMPARE(state.app(), QStringLiteral("Terminal"));

  // waitForStarted may complete before or after the output arrives. No remote client or repeated poll is needed.
  QVERIFY(SecureInputState::acceptsUpdates(ProcessState::Started, ConnectionState::Connecting));
  QVERIFY(!state.updateFromLogLine(active));
  QVERIFY(SecureInputState::acceptsUpdates(ProcessState::Started, ConnectionState::Listening));
  QVERIFY(state.isActive());
  QCOMPARE(state.app(), QStringLiteral("Terminal"));
}

QTEST_MAIN(SecureInputStateTests)
