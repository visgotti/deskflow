/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcClientTests.h"

#include "common/VersionInfo.h"
#include "gui/ipc/IpcClient.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTimer>
#include <QUuid>

using deskflow::gui::ipc::IpcClient;

namespace {

QString uniqueSocketName()
{
  // short: macOS caps unix socket paths at 104 bytes and $TMPDIR is already long
  return QStringLiteral("dfipc-%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
}

// Minimal stand-in for the core's ipc server: answers hello with a matching version.
class HelloServer : public QLocalServer
{
public:
  explicit HelloServer(QObject *parent = nullptr) : QLocalServer(parent)
  {
    connect(this, &QLocalServer::newConnection, this, [this] {
      while (auto *socket = nextPendingConnection()) {
        ++m_connections;
        m_lastSocket = socket;
        connect(socket, &QLocalSocket::readyRead, socket, [socket] {
          if (socket->readAll().startsWith("hello=")) {
            socket->write(QStringLiteral("hello=%1+%2\n").arg(kVersion, kVersionGitSha).toUtf8());
            socket->flush();
          }
        });
      }
    });
  }

  int m_connections = 0;
  QLocalSocket *m_lastSocket = nullptr;
};

} // namespace

void IpcClientTests::connectToServer_serverStartsListeningLate_connects()
{
  const auto name = uniqueSocketName();
  HelloServer server;
  IpcClient client(nullptr, name, QStringLiteral("test"));
  QSignalSpy connected(&client, &IpcClient::connected);
  QSignalSpy failed(&client, &IpcClient::connectionFailed);

  // the core starts its ipc server a moment after the gui first tries to connect
  QTimer::singleShot(100, &server, [&server, &name] { QVERIFY(server.listen(name)); });
  client.connectToServer();

  QVERIFY(connected.wait(2000));
  QCOMPARE(failed.count(), 0);
}

void IpcClientTests::connectedClient_serverDropsConnection_failsOnceWithoutReconnecting()
{
  const auto name = uniqueSocketName();
  HelloServer server;
  QVERIFY(server.listen(name));
  IpcClient client(nullptr, name, QStringLiteral("test"));
  QSignalSpy connected(&client, &IpcClient::connected);
  QSignalSpy failed(&client, &IpcClient::connectionFailed);

  client.connectToServer();
  QVERIFY(connected.wait(2000));
  QCOMPARE(server.m_connections, 1);

  server.m_lastSocket->abort();
  QVERIFY(failed.wait(2000));

  // reconnecting is the owner's call; the client itself must not start a retry
  // loop off a handler left over from its original connection attempt
  QTest::qWait(600);
  QCOMPARE(failed.count(), 1);
  QCOMPARE(server.m_connections, 1);
}

QTEST_MAIN(IpcClientTests)
