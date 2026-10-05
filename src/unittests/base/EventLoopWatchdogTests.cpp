/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "EventLoopWatchdogTests.h"

#include "base/EventLoopWatchdog.h"
#include "base/EventQueue.h"

#include <QTest>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

namespace {

constexpr auto kTimeout = 300ms;
constexpr auto kInterval = 20ms;

// runs an event loop on its own thread, watched the way the core watches its loop
class WatchedLoop
{
public:
  WatchedLoop()
  {
    m_thread = std::thread([this] {
      EventLoopWatchdog watchdog(
          &m_events, kTimeout,
          [this](EventLoopWatchdog::Clock::duration stalledFor) {
            m_stalledFor = stalledFor;
            ++m_stalls;
          },
          kInterval
      );
      m_events.loop();
    });
    m_events.waitForReady();
  }

  ~WatchedLoop()
  {
    m_events.addEvent(Event(EventTypes::Quit));
    m_thread.join();
  }

  // runs work on the loop's thread
  void post(std::function<void()> work)
  {
    m_events.addHandler(EventTypes::ClientConnected, this, [work = std::move(work)](const Event &) { work(); });
    m_events.addEvent(Event(EventTypes::ClientConnected, this));
  }

  int stalls() const
  {
    return m_stalls;
  }

  EventLoopWatchdog::Clock::duration stalledFor() const
  {
    return m_stalledFor;
  }

private:
  EventQueue m_events;
  std::atomic_int m_stalls = 0;
  std::atomic<EventLoopWatchdog::Clock::duration> m_stalledFor{};
  std::thread m_thread;
};

} // namespace

void EventLoopWatchdogTests::initTestCase()
{
  m_arch.init();
}

void EventLoopWatchdogTests::runningLoop_reportsNoStall()
{
  WatchedLoop loop;

  std::this_thread::sleep_for(kTimeout * 3);

  QCOMPARE(loop.stalls(), 0);
}

void EventLoopWatchdogTests::blockedLoop_reportsStallOnce()
{
  WatchedLoop loop;
  std::promise<void> release;
  loop.post([unblocked = release.get_future().share()] { unblocked.wait(); });

  QTRY_COMPARE_WITH_TIMEOUT(loop.stalls(), 1, 5000);
  QVERIFY(loop.stalledFor() >= kTimeout);

  // the loop is still stuck; it is reported once, not once per check
  std::this_thread::sleep_for(kTimeout * 2);
  QCOMPARE(loop.stalls(), 1);

  release.set_value();
}

void EventLoopWatchdogTests::blockedWhilePaused_reportsNoStall()
{
  WatchedLoop loop;
  std::promise<void> done;
  loop.post([&done] {
    {
      // e.g. resolving a host name on a slow network
      EventLoopWatchdog::Pause pause;
      std::this_thread::sleep_for(kTimeout * 3);
    }
    done.set_value();
  });

  QVERIFY(done.get_future().wait_for(5s) == std::future_status::ready);
  std::this_thread::sleep_for(kInterval * 5);

  QCOMPARE(loop.stalls(), 0);
}

void EventLoopWatchdogTests::destroyed_stopsWithoutReporting()
{
  EventQueue events;
  std::atomic_int stalls = 0;

  const auto start = EventLoopWatchdog::Clock::now();
  {
    // the loop never runs, so this would report a stall if it outlived its timeout
    EventLoopWatchdog watchdog(&events, 10s, [&stalls](auto) { ++stalls; }, 5s);
  }

  QVERIFY(EventLoopWatchdog::Clock::now() - start < 1s);
  QCOMPARE(stalls.load(), 0);
}

QTEST_MAIN(EventLoopWatchdogTests)
