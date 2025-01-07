#include "ParallaxGenRunner.hpp"

#include <cpptrace/from_current.hpp>

#include <mutex>
#include <spdlog/spdlog.h>
#include <stdexcept>

using namespace std;

ParallaxGenRunner::ParallaxGenRunner(const bool &MultiThread)
    : ThreadPool(std::thread::hardware_concurrency()), MultiThread(MultiThread), CompletedTasks(0) {}

void ParallaxGenRunner::addTask(const function<void()> &Task) { Tasks.push_back(Task); }

void ParallaxGenRunner::runTasks() {
  if (!MultiThread) {
    CPPTRACE_TRY {
      for (const auto &Task : Tasks) {
        Task();
        CompletedTasks.fetch_add(1);
      }
    }
    CPPTRACE_CATCH(const exception &E) {
      processException(E, cpptrace::from_current_exception().to_string(), false);
    }

    return;
  }

  std::atomic<bool> ExceptionThrown;
  std::exception Exception;
  std::string ExceptionStackTrace;
  std::mutex ExceptionMutex;

  // Multithreading only beyond this point
  for (const auto &Task : Tasks) {
    boost::asio::post(ThreadPool, [this, Task, &ExceptionThrown, &Exception, &ExceptionStackTrace, &ExceptionMutex] {
      if (ExceptionThrown.load()) {
        // Exception already thrown, don't run thread
        return;
      }

      CPPTRACE_TRY {
        Task();
        CompletedTasks.fetch_add(1);
      }
      CPPTRACE_CATCH(const exception &E) {
        if (!ExceptionThrown.load()) {
          lock_guard<mutex> Lock(ExceptionMutex);
          Exception = E;
          ExceptionStackTrace = cpptrace::from_current_exception().to_string();
          ExceptionThrown.store(true);
        }
      }
    });
  }

  while (true) {
    // Check if all tasks are done
    if (CompletedTasks.load() >= Tasks.size()) {
      // All tasks done
      break;
    }

    // If exception stop thread pool and throw
    if (ExceptionThrown.load()) {
      ThreadPool.stop();
      processException(Exception, ExceptionStackTrace, false);
    }

    // Sleep in between loops
    this_thread::sleep_for(chrono::milliseconds(10));
  }
}

void ParallaxGenRunner::processException(const exception &E, const string &StackTrace) {
  processException(E, StackTrace, true);
}

void ParallaxGenRunner::processException(const std::exception &E, const std::string &StackTrace, const bool &ExternalCaller) {
  if (string(E.what()) == "PGRUNNERINTERNAL") {
    // Internal exception, don't print
    return;
  }

  spdlog::critical("An unhandled exception occured. Please provide your full log in the bug report.");
  spdlog::critical(R"(Exception type: "{}" / Message: "{}")", typeid(E).name(), E.what());
  spdlog::critical(StackTrace);

  if (!ExternalCaller) {
    // Internal exception, throw on main thread
    throw runtime_error("PGRUNNERINTERNAL");
  }
}
