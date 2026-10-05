/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

class EventQueueTimer;
class IEventQueue;

/**
 * @brief Notices when an event loop stops running.
 *
 * A wedged event loop (e.g. two threads deadlocked on each other) can't notice
 * anything itself, because nothing on it runs: a client keeps looking connected
 * while it no longer reads from the server or injects input, and a server keeps
 * swallowing local input it never forwards. A periodic timer on the loop stamps
 * a heartbeat, and a separate thread reports a stall once the heartbeat is older
 * than the timeout. The stall handler runs once, on the watchdog thread.
 *
 * Construct it on the loop's thread before running the loop, and destroy it
 * there after the loop returns.
 */
class EventLoopWatchdog
{
public:
  using Clock = std::chrono::steady_clock;
  using StallHandler = std::function<void(Clock::duration stalledFor)>;

  EventLoopWatchdog(
      IEventQueue *events, Clock::duration timeout, StallHandler onStall,
      Clock::duration interval = std::chrono::seconds(1)
  );
  ~EventLoopWatchdog();

  EventLoopWatchdog(const EventLoopWatchdog &) = delete;
  EventLoopWatchdog &operator=(const EventLoopWatchdog &) = delete;

  /**
   * @brief Marks blocking work the loop is expected to do, such as resolving a
   * host name, which can outlast the timeout without the loop being stuck.
   *
   * The timeout starts again once the last pause ends. Pausing is process wide
   * and harmless when no watchdog is running.
   */
  class Pause
  {
  public:
    Pause();
    ~Pause();
    Pause(const Pause &) = delete;
    Pause &operator=(const Pause &) = delete;
  };

private:
  void beat();
  void watch();

  IEventQueue *m_events;
  const Clock::duration m_timeout;
  const Clock::duration m_interval;
  const StallHandler m_onStall;
  EventQueueTimer *m_timer = nullptr;
  std::atomic<Clock::rep> m_lastBeat;

  std::mutex m_mutex;
  std::condition_variable m_wake;
  bool m_stopping = false;
  std::thread m_thread;

  static std::atomic_int s_pauses;
};
