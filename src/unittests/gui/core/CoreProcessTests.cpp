/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreProcessTests.h"

#include "common/Constants.h"
#include "common/ExitCodes.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/CoreProcess.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>

using deskflow::core::ProcessState;
using deskflow::gui::CoreProcess;

void CoreProcessTests::initTestCase()
{
#if defined(Q_OS_WIN)
  QSKIP("the stand-in core is a shell script");
#endif

  // keep settings, and the core ipc socket (made in the temp dir), away from the
  // developer's own: the gui would otherwise talk to, and stop, their real core
  QVERIFY(m_dir.isValid());
  qputenv("XDG_CONFIG_HOME", m_dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", m_dir.path().toUtf8());
  qputenv("TMPDIR", m_dir.path().toUtf8());
  QCOMPARE(QDir::tempPath(), m_dir.path());

  // the gui runs the core from its own directory; this stand-in exits the first
  // time the way a core with a stalled event loop does, then stays up until stopped
  m_fakeCore = QStringLiteral("%1/%2").arg(QCoreApplication::applicationDirPath(), kCoreBinName);
  QFile script(m_fakeCore);
  QVERIFY(script.open(QFile::WriteOnly | QFile::Truncate));
  script.write(QStringLiteral("#!/bin/sh\n"
                              "ran=\"%1/fake-core-ran\"\n"
                              "if [ ! -e \"$ran\" ]; then touch \"$ran\"; exit %2; fi\n"
                              "trap 'exit 0' TERM\n"
                              "while :; do sleep 1; done\n")
                   .arg(m_dir.path())
                   .arg(s_exitStalled)
                   .toUtf8());
  script.close();
  QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
}

void CoreProcessTests::cleanupTestCase()
{
  if (!m_fakeCore.isEmpty()) {
    QFile::remove(m_fakeCore);
  }
}

void CoreProcessTests::stalledCoreExit_isRestarted()
{
  ServerConfig serverConfig;
  CoreProcess process(serverConfig);
  process.setMode(Settings::CoreMode::Client);
  QSignalSpy states(&process, &CoreProcess::processStateChanged);

  process.start(Settings::ProcessMode::Desktop);

  // the first core exits as stalled; the gui starts a new one by itself
  const QList<ProcessState> restarted = {
      ProcessState::Starting, ProcessState::Started, ProcessState::RetryPending, ProcessState::Starting,
      ProcessState::Started
  };
  const auto seen = [&states] {
    QList<ProcessState> list;
    for (const auto &args : std::as_const(states)) {
      list.append(args.at(0).value<ProcessState>());
    }
    return list;
  };
  QTRY_COMPARE_WITH_TIMEOUT(seen(), restarted, 10000);

  process.stop(Settings::ProcessMode::Desktop);
  QTRY_COMPARE_WITH_TIMEOUT(process.processState(), ProcessState::Stopped, 10000);
}

QTEST_MAIN(CoreProcessTests)
