/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2026 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/App.h"

#include "DisplayInvalidException.h"
#include "arch/Arch.h"
#include "base/Log.h"
#include "base/LogOutputters.h"
#include "common/ExitCodes.h"
#include "common/Settings.h"
#include "deskflow/DeskflowException.h"
#include "mt/ThreadException.h"

#if defined(Q_OS_WIN)
#include "base/IEventQueue.h"
#endif

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

#if defined(Q_OS_MACOS)
#include <ApplicationServices/ApplicationServices.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <array>
#include <crt_externs.h>
#include <ctime>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(WINAPI_XWINDOWS) or defined(WINAPI_LIBEI)
#include "platform/XDGPortalRegistry.h"
#endif

using namespace deskflow;

App *App::s_instance = nullptr;

//
// App
//

App::App(IEventQueue *events, const QString &processName)
    : m_bye(&exit),
      m_events(events),
      m_appUtil(events),
      m_pname(processName)
{
  assert(s_instance == nullptr);
  s_instance = this;
#if defined(WINAPI_XWINDOWS) or defined(WINAPI_LIBEI)
  deskflow::platform::setAppId();
#endif
}

App::~App()
{
  s_instance = nullptr;
}

void App::run(QThread &coreThread)
{
  LOG_INFO("starting core");

  // Important: Move the daemon app to the daemon thread before creating any more Qt objects
  // owned by the daemon app, as they will be created on the daemon thread.
  moveToThread(&coreThread);

  connect(&coreThread, &QThread::started, this, [this, &coreThread]() {
    LOG_DEBUG("core thread started");

#if MAC_OS_X_VERSION_10_7
    // dock hide only supported on lion :(
    ProcessSerialNumber psn = {0, kCurrentProcess};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetCurrentProcess(&psn);
#pragma GCC diagnostic pop

    TransformProcessType(&psn, kProcessTransformToBackgroundApplication);
#endif

    // install application in to arch
    appUtil().adoptApp(this);

    // HACK: fail by default (saves us setting result in each catch)
    int result = s_exitFailed;

    try {
      result = appUtil().run();
    } catch (ExitAppException &e) {
      // instead of showing a nasty error, just exit with the error code.
      // not sure if i like this behaviour, but it's probably better than
      // using the exit(int) function!
      result = e.getCode();
    } catch (DisplayInvalidException &die) {
      LOG_CRIT("a display invalid exception error occurred: %s\n", die.what());
      // display invalid exceptions can occur when going to sleep. When this
      // process exits, the UI will restart us instantly. We don't really want
      // that behevior, so we quies for a bit
      Arch::sleep(10);
    } catch (std::runtime_error &re) {
      LOG_CRIT("a runtime error occurred: %s\n", re.what());
    } catch (std::exception &e) {
      LOG_CRIT("an error occurred: %s\n", e.what());
    } catch (...) {
      LOG_CRIT("an unknown error occurred\n");
    }

    if (result == s_exitSuccess) {
      LOG_INFO("core stopped successfully");
    } else {
      updateExitCode(result);
      LOG_ERR("core stopped with error code: %d", result);
    }

    coreThread.quit();
    LOG_DEBUG("core thread finished");
  });

  LOG_DEBUG("starting core thread");
  coreThread.start();
}

void App::setupFileLogging()
{
  if (Settings::value(Settings::Log::ToFile).toBool()) {
    const auto file = Settings::value(Settings::Log::File).toString();
    m_fileLog = new FileLogOutputter(file); // NOSONAR - Adopted by `Log`
    CLOG->insert(m_fileLog);
    LOG_VERBOSE("logging to file (%s) enabled", qPrintable(file));
  }
}

void App::loggingFilterWarning() const
{
  if ((CLOG->getFilter() > CLOG->getConsoleMaxLevel()) && (Settings::value(Settings::Log::ToFile).toBool())) {
    LOG_WARN(
        "log messages above %s are NOT sent to console (use file logging)",
        qPrintable(LogLevel::toOption(CLOG->getConsoleMaxLevel()))
    );
  }
}

void App::initApp()
{
  parseArgs();

  // set log filter
  if (const auto logLevel = Settings::logLevelText(); !CLOG->setFilter(logLevel)) {
    LOG_CRIT(
        "%s: unrecognized log level `%s'" BYE, qPrintable(processName()), qPrintable(logLevel),
        qPrintable(processName())
    );
    m_bye(s_exitArgs);
  }
  loggingFilterWarning();

  // setup file logging after parsing args
  setupFileLogging();

  // load configuration
  loadConfig();
}

void App::handleScreenError() const
{
  LOG_CRIT("error on screen");
  getEvents()->addEvent(Event(EventTypes::Quit));
}

void App::quit() const
{
  LOG_INFO("quitting");
  getEvents()->addEvent(Event(EventTypes::Quit));
}

void App::runEventsLoop(const void *)
{
  int exitCode = m_events->loop();
  if (exitCode != s_exitSuccess) {
    throw ThreadExitException(new LoopErrorCode(exitCode));
  }
}

namespace {

// longer than anything the loop legitimately blocks on (host name lookups pause
// the watchdog), and well past the 9 s after which the other end has already
// given up on us
constexpr auto kEventLoopStallTimeout = std::chrono::seconds(15);

// the stuck thread may hold the log lock, so the report and the log line get
// this long before the core exits regardless
constexpr auto kStallReportBudget = std::chrono::seconds(10);

#if defined(Q_OS_MACOS)
const auto kStallReportPattern = QStringLiteral("deskflow-core-stall-*.txt");
constexpr qsizetype kStallReportsKept = 5;

QString stallReportDir()
{
  if (Settings::value(Settings::Log::ToFile).toBool()) {
    return QFileInfo(Settings::value(Settings::Log::File).toString()).absolutePath();
  }
  return QDir::tempPath();
}

void pruneStallReports(const QString &dir)
{
  const auto reports = QDir(dir).entryInfoList({kStallReportPattern}, QDir::Files, QDir::Time);
  for (qsizetype i = kStallReportsKept; i < reports.size(); ++i) {
    QFile::remove(reports.at(i).absoluteFilePath());
  }
}

// samples every thread of this process for a second with the system's sample
// tool, which needs none of our locks
void sampleThreads(const std::string &path)
{
  const auto pid = std::to_string(getpid());
  const char *argv[] = {"/usr/bin/sample", pid.c_str(), "1", "-mayDie", "-file", path.c_str(), nullptr};

  posix_spawn_file_actions_t quiet;
  posix_spawn_file_actions_init(&quiet);
  posix_spawn_file_actions_addopen(&quiet, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addopen(&quiet, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  if (pid_t child = 0;
      posix_spawn(&child, argv[0], &quiet, nullptr, const_cast<char *const *>(argv), *_NSGetEnviron()) == 0) {
    int status = 0;
    waitpid(child, &status, 0);
  }
  posix_spawn_file_actions_destroy(&quiet);
}
#endif

[[noreturn]] void exitStalledCore(EventLoopWatchdog::Clock::duration stalledFor, const std::string &reportDir)
{
  std::thread([] {
    std::this_thread::sleep_for(kStallReportBudget);
    std::_Exit(s_exitStalled);
  }).detach();

  const auto seconds = static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(stalledFor).count());

#if defined(Q_OS_MACOS)
  // where each thread is stuck is the only evidence of what wedged the loop
  std::array<char, 32> stamp{};
  const auto now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  std::strftime(stamp.data(), stamp.size(), "%Y%m%d-%H%M%S", &local);
  const auto report = reportDir + "/deskflow-core-stall-" + stamp.data() + ".txt";
  sampleThreads(report);
  LOG_CRIT(
      "event loop stopped responding %lld s ago, thread stacks saved to: %s; exiting so the core is restarted", seconds,
      report.c_str()
  );
#else
  LOG_CRIT("event loop stopped responding %lld s ago; exiting so the core is restarted", seconds);
#endif

  std::_Exit(s_exitStalled);
}

} // namespace

std::unique_ptr<EventLoopWatchdog> App::watchEventLoop() const
{
#if defined(Q_OS_MACOS)
  const auto reportDir = stallReportDir();
  pruneStallReports(reportDir);
#else
  const QString reportDir;
#endif

  return std::make_unique<EventLoopWatchdog>(
      m_events, kEventLoopStallTimeout,
      [reportDir = reportDir.toStdString()](EventLoopWatchdog::Clock::duration stalledFor) {
        exitStalledCore(stalledFor, reportDir);
      }
  );
}
