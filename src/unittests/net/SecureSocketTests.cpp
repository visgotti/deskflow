/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SecureSocketTests.h"

#include "base/EventQueue.h"
#include "common/Settings.h"
#include "mt/Lock.h"
#include "net/NetworkAddress.h"
#include "net/SecureSocket.h"
#include "net/SecureUtils.h"
#include "net/SocketMultiplexer.h"

#include <QTcpServer>
#include <QTcpSocket>

#include <chrono>
#include <future>
#include <memory>
#include <thread>

namespace {

// gives the test the socket lock that the multiplexer's jobs take first, so it can hold a
// job at a known point
class ExposedSecureSocket : public SecureSocket
{
public:
  using SecureSocket::SecureSocket;
  using TCPSocket::getMutex;
  using TCPSocket::isConnected;
};

} // namespace

void SecureSocketTests::initTestCase()
{
  // keep deskflow::Settings away from the developer's real config and state files
  QVERIFY(m_settingsDir.isValid());
  qputenv("XDG_CONFIG_HOME", m_settingsDir.path().toUtf8());
  qputenv("XDG_STATE_HOME", m_settingsDir.path().toUtf8());
  m_arch.init();

  const auto certificate = m_settingsDir.filePath(QStringLiteral("deskflow.pem"));
  deskflow::generatePemSelfSignedCert(certificate);
  Settings::setValue(Settings::Security::Certificate, certificate);
}

void SecureSocketTests::deleteWhileHandshakeJobIsQueued_doesNotDeadlock()
{
  // The client times out a slow tls handshake by deleting its socket on the event loop thread,
  // while the multiplexer thread may already be running that socket's handshake job. Deleting
  // took the ssl lock and then waited for the multiplexer to finish its pass; the job it was
  // waiting for needs the ssl lock: both threads stopped for good, and the client sat
  // "connected" until it was restarted by hand.
  EventQueue events;
  SocketMultiplexer multiplexer;

  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));

  auto *socket =
      new ExposedSecureSocket(&events, &multiplexer, IArchNetwork::AddressFamily::INet, SecurityLevel::Encrypted);
  socket->initSsl(false);
  NetworkAddress address("127.0.0.1", server.serverPort());
  address.resolve();
  socket->connect(address);

  QVERIFY(server.waitForNewConnection(5000));
  std::unique_ptr<QTcpSocket> peer(server.nextPendingConnection());
  QTRY_VERIFY_WITH_TIMEOUT(socket->isConnected(), 5000);

  // start the handshake; once the client hello arrives the job is waiting for the server's answer
  socket->secureConnect();
  QVERIFY(peer->waitForReadyRead(5000));

  // hold the job at its first step: the header of a large handshake record wakes it, and it
  // then waits for the socket lock while the multiplexer keeps its job list
  auto heldJob = std::make_unique<Lock>(&socket->getMutex());
  const char recordHeader[] = {0x16, 0x03, 0x03, 0x40, 0x00};
  peer->write(recordHeader, sizeof(recordHeader));
  QVERIFY(peer->waitForBytesWritten(5000));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // delete the socket the way a connect timeout does, and give it time to reach the multiplexer
  auto deleted = std::async(std::launch::async, [socket] { delete socket; });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // let the job run on: it now needs the ssl lock
  heldJob.reset();

  if (deleted.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
    // the threads can't be unwound, so stop here rather than hang in teardown
    qFatal("deleting a secure socket deadlocked with the socket multiplexer thread");
  }
}

QTEST_MAIN(SecureSocketTests)
