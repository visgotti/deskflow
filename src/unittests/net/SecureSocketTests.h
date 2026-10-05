/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class SecureSocketTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void deleteWhileHandshakeJobIsQueued_doesNotDeadlock();

private:
  Arch m_arch;
  Log m_log;
  QTemporaryDir m_settingsDir;
};
