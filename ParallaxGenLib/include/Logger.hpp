#pragma once

#include <spdlog/spdlog.h>

#include <fmt/format.h>
#include <string>
#include <vector>

class Logger {
private:
    thread_local static std::vector<std::wstring> s_prefixStack;
    static auto buildPrefixWString() -> std::wstring;
    static auto buildPrefixString() -> std::string;

public:
    // Scoped prefix class
    class Prefix {
    public:
        explicit Prefix(const std::wstring& prefix);
        explicit Prefix(const std::string& prefix);
        ~Prefix();
        Prefix(const Prefix&) = delete;
        auto operator=(const Prefix&) -> Prefix& = delete;
        Prefix(Prefix&&) = delete;
        auto operator=(Prefix&&) -> Prefix& = delete;
    };

    // WString Log functions
    template <typename... Args> static void critical(const std::wstring& fmt, Args&&... moreArgs)
    {
        spdlog::critical(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        exit(1);
    }

    template <typename... Args> static void error(const std::wstring& fmt, Args&&... moreArgs)
    {
        spdlog::error(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void warn(const std::wstring& fmt, Args&&... moreArgs)
    {
        spdlog::warn(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void info(const std::wstring& fmt, Args&&... moreArgs)
    {
        spdlog::info(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void debug(const std::wstring& fmt, Args&&... moreArgs)
    {
        spdlog::debug(fmt::runtime(buildPrefixWString() + fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void trace(const std::wstring& fmt, Args&&... moreArgs)
    {
        spdlog::trace(fmt::runtime(buildPrefixWString() + fmt), std::forward<Args>(moreArgs)...);
    }

    // String Log functions
    template <typename... Args> static void critical(const std::string& fmt, Args&&... moreArgs)
    {
        spdlog::critical(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        exit(1);
    }

    template <typename... Args> static void error(const std::string& fmt, Args&&... moreArgs)
    {
        spdlog::error(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void warn(const std::string& fmt, Args&&... moreArgs)
    {
        spdlog::warn(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void info(const std::string& fmt, Args&&... moreArgs)
    {
        spdlog::info(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void debug(const std::string& fmt, Args&&... moreArgs)
    {
        spdlog::debug(fmt::runtime(buildPrefixString() + fmt), std::forward<Args>(moreArgs)...);
    }

    template <typename... Args> static void trace(const std::string& fmt, Args&&... moreArgs)
    {
        spdlog::trace(fmt::runtime(buildPrefixString() + fmt), std::forward<Args>(moreArgs)...);
    }
};
