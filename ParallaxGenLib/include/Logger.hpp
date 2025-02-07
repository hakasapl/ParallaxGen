#pragma once

#include <mutex>
#include <spdlog/spdlog.h>

#include <fmt/format.h>
#include <string>
#include <unordered_set>
#include <vector>

class Logger {
private:
    inline static std::unordered_set<std::wstring> s_existingMessages;
    inline static std::mutex s_existingMessagesMutex;

    thread_local static std::vector<std::wstring> s_prefixStack;
    static auto buildPrefixWString() -> std::wstring;
    static auto buildPrefixString() -> std::string;

    template <typename... Args> static auto shouldLogString(const std::wstring& fmt, Args&&... moreArgs) -> bool
    {
        const std::lock_guard<std::mutex> lock(s_existingMessagesMutex);
        const auto resultLog = fmt::format(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        if (s_existingMessages.find(resultLog) != s_existingMessages.end()) {
            // don't log anything if already logged
            return false;
        }
        s_existingMessages.insert(resultLog);
        return true;
    }

    template <typename... Args> static auto shouldLogString(const std::string& fmt, Args&&... moreArgs) -> bool
    {
        const std::lock_guard<std::mutex> lock(s_existingMessagesMutex);
        const std::string resolvedNarrow = fmt::format(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        const std::wstring resolvedWide(resolvedNarrow.begin(), resolvedNarrow.end());
        if (s_existingMessages.find(resolvedWide) != s_existingMessages.end()) {
            // don't log anything if already logged
            return false;
        }
        s_existingMessages.insert(resolvedWide);
        return true;
    }

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
        if (shouldLogString(fmt, std::forward<Args>(moreArgs)...)) {
            spdlog::error(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        }
    }

    template <typename... Args> static void warn(const std::wstring& fmt, Args&&... moreArgs)
    {
        if (shouldLogString(fmt, std::forward<Args>(moreArgs)...)) {
            spdlog::warn(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        }
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
        if (shouldLogString(fmt, std::forward<Args>(moreArgs)...)) {
            spdlog::error(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        }
    }

    template <typename... Args> static void warn(const std::string& fmt, Args&&... moreArgs)
    {
        if (shouldLogString(fmt, std::forward<Args>(moreArgs)...)) {
            spdlog::warn(fmt::runtime(fmt), std::forward<Args>(moreArgs)...);
        }
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
