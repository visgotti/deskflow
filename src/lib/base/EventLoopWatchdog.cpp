/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/EventLoopWatchdog.h"

#include "base/IEventQueue.h"

std::atomic_int EventLoopWatchdog::s_pauses = 0;

EventLoopWatchdog::Pause::Pause()
{
  ++s_pauses;
}

EventLoopWatchdog::Pause::~Pause()
{
  --s_pauses;
}

EventLoopWatchdog::EventLoopWatchdog(
    IEventQueue *events, Clock::duration timeout, StallHandler onStall, Clock::duration interval
)
    : m_events(events),
      m_timeout(timeout),
      m_interval(interval),
      m_onStall(std::move(onStall)),
      m_lastBeat(Clock::now().time_since_epoch().count())
{
  m_timer = m_events->newTimer(std::chrono::duration<double>(m_interval).count(), nullptr);
  m_events->addHandler(EventTypes::Timer, m_timer, [this](const auto &) { beat(); });
  m_thread = std::thread(&EventLoopWatchdog::watch, this);
}

EventLoopWatchdog::~EventLoopWatchdog()
{
  {
    std::scoped_lock lock{m_mutex};
    m_stopping = true;
  }
  m_wake.notify_all();
  m_thread.join();

  m_events->removeHandler(EventTypes::Timer, m_timer);
  m_events->deleteTimer(m_timer);
}

void EventLoopWatchdog::beat()
{
  m_lastBeat = Clock::now().time_since_epoch().count();
}

void EventLoopWatchdog::watch()
{
  auto previousCheck = Clock::now();

  std::unique_lock lock{m_mutex};
  while (!m_wake.wait_for(lock, m_interval, [this] { return m_stopping; })) {
    const auto now = Clock::now();

    // a check that comes a whole timeout late means this thread didn't run either
    // (the process was stopped, e.g. by a debugger, or the clock counts system
    // sleep), so the loop had no chance to beat: count from now instead
    if (s_pauses > 0 || now - previousCheck >= m_timeout) {
      beat();
    }
    previousCheck = now;

    if (const auto stalledFor = now - Clock::time_point(Clock::duration(m_lastBeat)); stalledFor >= m_timeout) {
      lock.unlock();
      m_onStall(stalledFor);
      return;
    }
  }
}
