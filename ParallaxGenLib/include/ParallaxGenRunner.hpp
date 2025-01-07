#pragma once

#include <boost/asio.hpp>
#include <exception>

class ParallaxGenRunner {
private:
  boost::asio::thread_pool ThreadPool; /** Thread pool responsible for queueing tasks for multithreaded */

  const bool MultiThread; /** If true, run multithreaded */

  std::vector<std::function<void()>> Tasks; /** Task list to run */
  std::atomic<size_t> CompletedTasks; /** Counter of completed tasks */

public:
  /**
   * @brief Construct a new Parallax Gen Runner object
   *
   * @param MultiThread if true, use multithreading
   */
  ParallaxGenRunner(const bool &MultiThread = true);

  /**
   * @brief Add a task to the task list
   *
   * @param Task Task to add (function<void()>)
   */
  void addTask(const std::function<void()> &Task);

  /**
   * @brief Blocking function that runs all tasks in the task list. Intended to be run from the main thread
   */
  void runTasks();

  /**
   * @brief Process an exception - prints stack trace and exception message, and throws a main thread exception (for external callers)
   *
   * @param E Exception to process
   * @param StackTrace Stack trace of the exception
   */
  static void processException(const std::exception &E, const std::string &StackTrace);

private:
  /**
   * @brief Process an exception - prints stack trace and exception message, and throws a main thread exception
   *
   * @param E Exception to process
   * @param StackTrace Stack trace of the exception
   * @param ExternalCaller Set to false for internal, true for external
   */
  static void processException(const std::exception &E, const std::string &StackTrace, const bool &ExternalCaller);
};
