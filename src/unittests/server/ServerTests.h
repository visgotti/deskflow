/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class ServerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void SwitchToScreenInfo_alloc_screen();
  void KeyboardBroadcastInfo_alloc_stateAndSceens();
  void clientDisconnect_saverPulledCursorHomeThenUserReturned_reentersPrimary();
  void clientDisconnect_saverActivatedOnPrimaryNeverDeactivated_reentersPrimary();
  void clientDisconnect_duringScreensaver_staysHomeWhenSaverEnds();
  void clientDisconnect_duringSwitchDelay_reconnectedClientIsTheOneSwitchedTo();
  void clientProxy_emptyShapeAfterHandshake_isStillAcknowledged();

private:
  Arch m_arch;
  Log m_log;
  QTemporaryDir m_settingsDir;
};
