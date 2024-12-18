#pragma once

#include <spdlog/spdlog.h>

#include <fmt/format.h>
#include <string>
#include <vector>

class Logger {
private:
  thread_local static std::vector<std::wstring> PrefixStack;
  static auto buildPrefixString() -> std::wstring;

public:
  // Scoped prefix class
  class Prefix {
  public:
    explicit Prefix(const std::wstring &Prefix);
    ~Prefix();
    Prefix(const Prefix &) = delete;
    auto operator=(const Prefix &) -> Prefix & = delete;
    Prefix(Prefix &&) = delete;
    auto operator=(Prefix &&) -> Prefix & = delete;
  };

  // Log functions
  template <typename... Args> static void critical(const std::wstring &Fmt, Args &&...MoreArgs) {
    spdlog::critical(fmt::runtime(buildPrefixString() + Fmt), std::forward<Args>(MoreArgs)...);
  }

  template <typename... Args> static void error(const std::wstring &Fmt, Args &&...MoreArgs) {
    spdlog::error(fmt::runtime(buildPrefixString() + Fmt), std::forward<Args>(MoreArgs)...);
  }

  template <typename... Args> static void warn(const std::wstring &Fmt, Args &&...MoreArgs) {
    spdlog::warn(fmt::runtime(buildPrefixString() + Fmt), std::forward<Args>(MoreArgs)...);
  }

  template <typename... Args> static void info(const std::wstring &Fmt, Args &&...MoreArgs) {
    spdlog::info(fmt::runtime(buildPrefixString() + Fmt), std::forward<Args>(MoreArgs)...);
  }

  template <typename... Args> static void debug(const std::wstring &Fmt, Args &&...MoreArgs) {
    spdlog::debug(fmt::runtime(buildPrefixString() + Fmt), std::forward<Args>(MoreArgs)...);
  }

  template <typename... Args> static void trace(const std::wstring &Fmt, Args &&...MoreArgs) {
    spdlog::trace(fmt::runtime(buildPrefixString() + Fmt), std::forward<Args>(MoreArgs)...);
  }
};
