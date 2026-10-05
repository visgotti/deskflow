/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "EventQueueTests.h"

#include "base/EventQueue.h"

#include <QTest>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

void EventQueueTests::initTestCase()
{
  m_arch.init();
}

void EventQueueTests::dispatchEvent_noHandler_returnsFalse()
{
  EventQueue events;

  QVERIFY(!events.dispatchEvent(Event(EventTypes::ClientDisconnected, this)));
}

void EventQueueTests::dispatchEvent_noTypeHandler_dispatchesUnknownHandler()
{
  EventQueue events;
  bool fallbackCalled = false;
  events.addHandler(EventTypes::Unknown, this, [&fallbackCalled](const Event &) { fallbackCalled = true; });

  QVERIFY(events.dispatchEvent(Event(EventTypes::ClientDisconnected, this)));
  QVERIFY(fallbackCalled);
}

void EventQueueTests::dispatchEvent_handlerRemovesItself_keepsHandlerAliveUntilReturn()
{
  EventQueue events;
  auto handlerLifetime = std::make_shared<int>(1);
  std::weak_ptr<int> handlerLifetimeObserver = handlerLifetime;
  bool handlerAliveAfterRemoval = false;

  events.addHandler(
      EventTypes::ClientDisconnected, this,
      [this, &events, &handlerLifetimeObserver, &handlerAliveAfterRemoval, handlerLifetime](const Event &) {
        events.removeHandler(EventTypes::ClientDisconnected, this);
        handlerAliveAfterRemoval = handlerLifetime != nullptr && !handlerLifetimeObserver.expired();
      }
  );
  handlerLifetime.reset();

  QVERIFY(events.dispatchEvent(Event(EventTypes::ClientDisconnected, this)));
  QVERIFY(handlerAliveAfterRemoval);
  QVERIFY(handlerLifetimeObserver.expired());
}

void EventQueueTests::timer_subSecondInterval_firesEveryInterval()
{
  // timers count down with Arch::time(); when that returned whole seconds a
  // 20 ms timer only fired on second boundaries, at most once a second
  EventQueue events;
  std::atomic_int fired = 0;
  std::promise<void> started;

  std::thread loop([&events, &fired, &started] {
    auto *timer = events.newTimer(0.02, nullptr);
    events.addHandler(EventTypes::Timer, timer, [&fired](const Event &) { ++fired; });
    events.addHandler(EventTypes::ClientConnected, &started, [&started](const Event &) { started.set_value(); });
    events.addEvent(Event(EventTypes::ClientConnected, &started));
    events.loop();
    events.removeHandler(EventTypes::Timer, timer);
    events.deleteTimer(timer);
  });
  started.get_future().wait();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  const int firedInHalfASecond = fired;

  events.addEvent(Event(EventTypes::Quit));
  loop.join();

  QVERIFY2(firedInHalfASecond >= 10, qPrintable(QStringLiteral("fired %1 times").arg(firedInHalfASecond)));
}

QTEST_MAIN(EventQueueTests)
