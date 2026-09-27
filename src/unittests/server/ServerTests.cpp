/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

#include "base/EventQueue.h"
#include "deskflow/IPlatformScreen.h"
#include "deskflow/Screen.h"
#include "deskflow/ipc/CoreIpcServer.h"
#include "server/BaseClientProxy.h"
#include "server/PrimaryClient.h"
#include "server/Server.h"

#include <memory>

namespace {

// Stands in for OSXScreen & co: only tracks whether the server's own screen
// has been entered (on a real Mac, "not entered" means the event tap swallows
// every local click and key).
class FakePrimaryScreen : public IPlatformScreen
{
public:
  explicit FakePrimaryScreen(const IEventQueue *events) : IPlatformScreen(events)
  {
  }

  bool m_entered = true;

  void enable() override
  {
  }
  void disable() override
  {
  }
  void enter() override
  {
    m_entered = true;
  }
  bool canLeave() override
  {
    return true;
  }
  void leave() override
  {
    m_entered = false;
  }
  bool setClipboard(ClipboardID, const IClipboard *) override
  {
    return true;
  }
  void checkClipboards() override
  {
  }
  void openScreensaver(bool) override
  {
  }
  void closeScreensaver() override
  {
  }
  void screensaver(bool) override
  {
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &) override
  {
  }
  void setSequenceNumber(uint32_t) override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }
  bool isPrimary() const override
  {
    return true;
  }
  void *getEventTarget() const override
  {
    return const_cast<FakePrimaryScreen *>(this);
  }
  bool getClipboard(ClipboardID, IClipboard *) const override
  {
    return false;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = 0;
    y = 0;
    w = 1920;
    h = 1080;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    getCursorCenter(x, y);
  }
  void reconfigure(uint32_t) override
  {
  }
  uint32_t activeSides() override
  {
    return 0;
  }
  void warpCursor(int32_t, int32_t) override
  {
  }
  uint32_t registerHotKey(KeyID, KeyModifierMask) override
  {
    return 0;
  }
  void unregisterHotKey(uint32_t) override
  {
  }
  void fakeInputBegin() override
  {
  }
  void fakeInputEnd() override
  {
  }
  int32_t getJumpZoneSize() const override
  {
    return 1;
  }
  bool isAnyMouseButtonDown(uint32_t &) const override
  {
    return false;
  }
  void getCursorCenter(int32_t &x, int32_t &y) const override
  {
    x = 960;
    y = 540;
  }
  void fakeMouseButton(ButtonID, bool) override
  {
  }
  void fakeMouseMove(int32_t, int32_t) override
  {
  }
  void fakeMouseRelativeMove(int32_t, int32_t) const override
  {
  }
  void fakeMouseWheel(ScrollDelta) const override
  {
  }
  void updateKeyMap() override
  {
  }
  void updateKeyState() override
  {
  }
  void setHalfDuplexMask(KeyModifierMask) override
  {
  }
  void fakeKeyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }
  bool fakeKeyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
    return false;
  }
  bool fakeKeyUp(KeyButton) override
  {
    return false;
  }
  void fakeAllKeysUp() override
  {
  }
  bool fakeCtrlAltDel() override
  {
    return false;
  }
  bool isKeyDown(KeyButton) const override
  {
    return false;
  }
  KeyModifierMask getActiveModifiers() const override
  {
    return 0;
  }
  KeyModifierMask pollActiveModifiers() const override
  {
    return 0;
  }
  int32_t pollActiveGroup() const override
  {
    return 0;
  }
  void pollPressedKeys(KeyButtonSet &) const override
  {
  }

protected:
  void handleSystemEvent(const Event &) override
  {
  }
};

class FakeClientProxy : public BaseClientProxy
{
public:
  using BaseClientProxy::BaseClientProxy;

  void *getEventTarget() const override
  {
    return static_cast<BaseClientProxy *>(const_cast<FakeClientProxy *>(this));
  }
  bool getClipboard(ClipboardID, IClipboard *) const override
  {
    return false;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = 0;
    y = 0;
    w = 1920;
    h = 1080;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = 0;
    y = 0;
  }
  void enter(int32_t, int32_t, uint32_t, KeyModifierMask, bool) override
  {
  }
  bool leave() override
  {
    return true;
  }
  void setClipboard(ClipboardID, const IClipboard *) override
  {
  }
  void grabClipboard(ClipboardID) override
  {
  }
  void setClipboardDirty(ClipboardID, bool) override
  {
  }
  void keyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }
  void keyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
  }
  void keyUp(KeyID, KeyModifierMask, KeyButton) override
  {
  }
  void mouseDown(ButtonID) override
  {
  }
  void mouseUp(ButtonID) override
  {
  }
  void mouseMove(int32_t, int32_t) override
  {
  }
  void mouseRelativeMove(int32_t, int32_t) override
  {
  }
  void mouseWheel(int32_t, int32_t) override
  {
  }
  void screensaver(bool) override
  {
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &) override
  {
  }
  void sendDragInfo(uint32_t, const char *, size_t) override
  {
  }
  void fileChunkSending(uint8_t, char *, size_t) override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }
  void secureInputNotification(const std::string &) const override
  {
  }
  deskflow::IStream *getStream() const override
  {
    return nullptr;
  }
};

// A server ("server") with one client ("client") to its right, wired through
// the real Server, PrimaryClient and Screen so screen-switch bookkeeping is
// exercised end to end.
struct ServerHarness
{
  EventQueue events;
  deskflow::server::Config config{&events};
  FakePrimaryScreen *platform = new FakePrimaryScreen(&events); // owned by screen
  deskflow::Screen screen{platform, &events};
  PrimaryClient primary{"server", &screen};
  std::unique_ptr<Server> server;
  FakeClientProxy *client = new FakeClientProxy("client"); // deleted by server on disconnect

  ServerHarness()
  {
    config.addScreen("server");
    config.addScreen("client");
    config.connect("server", Direction::Right, 0.0f, 1.0f, "client", 0.0f, 1.0f);
    config.connect("client", Direction::Left, 0.0f, 1.0f, "server", 0.0f, 1.0f);
    server = std::make_unique<Server>(config, &primary, &screen, &events);
    server->adoptClient(client);
  }

  void switchTo(const std::string &name)
  {
    Server::SwitchToScreenInfo info(name);
    events.dispatchEvent(
        Event(EventTypes::ServerSwitchToScreen, config.getInputFilter(), &info, Event::EventFlags::DontFreeData)
    );
  }

  void screensaverActivated()
  {
    events.dispatchEvent(Event(EventTypes::PrimaryScreenSaverActivated, primary.getEventTarget()));
  }

  void screensaverDeactivated()
  {
    events.dispatchEvent(Event(EventTypes::PrimaryScreenSaverDeactivated, primary.getEventTarget()));
  }

  void clientDisconnected()
  {
    events.dispatchEvent(Event(EventTypes::ClientProxyDisconnected, client->getEventTarget()));
    client = nullptr;
  }
};

} // namespace

void ServerTests::initTestCase()
{
  // keep deskflow::Settings away from the developer's real config and state files
  QVERIFY(m_settingsDir.isValid());
  qputenv("XDG_CONFIG_HOME", m_settingsDir.path().toUtf8());
  qputenv("XDG_STATE_HOME", m_settingsDir.path().toUtf8());
  m_arch.init();

  // Server reports connection state over core IPC; it only needs the instance, not a listener
  new deskflow::core::ipc::CoreIpcServer(this); // NOSONAR - Qt managed
}

void ServerTests::clientDisconnect_saverPulledCursorHomeThenUserReturned_reentersPrimary()
{
  ServerHarness h;

  h.switchTo("client");
  QVERIFY(!h.platform->m_entered);

  // the server's screensaver starts while the cursor is on the client: the
  // server pulls the cursor home. the matching "deactivated" never arrives.
  h.screensaverActivated();
  QVERIFY(h.platform->m_entered);

  // the user comes back and moves onto the client again, then it drops
  h.switchTo("client");
  QVERIFY(!h.platform->m_entered);
  h.clientDisconnected();

  // with no clients left the cursor can only be on the server, so its screen
  // must be entered again or it keeps swallowing all local input
  QVERIFY(h.platform->m_entered);
  QVERIFY(h.screen.isOnScreen());
}

void ServerTests::clientDisconnect_saverActivatedOnPrimaryNeverDeactivated_reentersPrimary()
{
  ServerHarness h;

  // screensaver starts while the cursor is on the server; "deactivated" never arrives
  h.screensaverActivated();
  QVERIFY(h.platform->m_entered);

  h.switchTo("client");
  QVERIFY(!h.platform->m_entered);
  h.clientDisconnected();

  QVERIFY(h.platform->m_entered);
  QVERIFY(h.screen.isOnScreen());
}

void ServerTests::clientDisconnect_duringScreensaver_staysHomeWhenSaverEnds()
{
  ServerHarness h;

  h.switchTo("client");
  h.screensaverActivated();
  QVERIFY(h.platform->m_entered);

  // the client drops while the saver has the cursor parked at home, so there
  // is nothing to return to once the saver ends
  h.clientDisconnected();
  QVERIFY(h.platform->m_entered);

  h.screensaverDeactivated();
  QVERIFY(h.platform->m_entered);
  QVERIFY(h.screen.isOnScreen());
}

void ServerTests::SwitchToScreenInfo_alloc_screen()
{
  auto actual = new Server::SwitchToScreenInfo("test");
  QCOMPARE(actual->m_screen, "test");
  delete actual;
}

void ServerTests::KeyboardBroadcastInfo_alloc_stateAndSceens()
{
  auto info = new Server::KeyboardBroadcastInfo(Server::KeyboardBroadcastInfo::State::kOn, "test");
  QCOMPARE(info->m_state, Server::KeyboardBroadcastInfo::State::kOn);
  QCOMPARE(info->m_screens, "test");
  delete info;
}

QTEST_MAIN(ServerTests)
