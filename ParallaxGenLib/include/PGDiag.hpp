#pragma once

#include "ParallaxGenUtil.hpp"

#include <nlohmann/json.hpp>

#include <mutex>
#include <string>

#include <spdlog/spdlog.h>

class PGDiag {
private:
    inline static nlohmann::json s_diagJSON;
    inline static std::mutex s_diagJSONMutex;
    inline static bool s_enabled = false;

    thread_local static std::vector<nlohmann::json*> s_ptrPrefixStack;

public:
    // Scoped prefix class
    class Prefix {
    public:
        explicit Prefix(const std::wstring& prefix, const nlohmann::json::value_t& type);
        explicit Prefix(const std::string& prefix, const nlohmann::json::value_t& type);
        ~Prefix();
        Prefix(const Prefix&) = delete;
        auto operator=(const Prefix&) -> Prefix& = delete;
        Prefix(Prefix&&) = delete;
        auto operator=(Prefix&&) -> Prefix& = delete;
    };

    static void init();
    static auto getJSON() -> nlohmann::json;

    template <typename T> static void insert(const std::wstring& key, const T& value)
    {
        insert(ParallaxGenUtil::utf16toUTF8(key), value);
    }

    template <typename T> static void insert(const std::string& key, const T& value)
    {
        const std::lock_guard<std::mutex> lock(s_diagJSONMutex);

        if (!s_enabled) {
            return;
        }

        if (s_ptrPrefixStack.empty()) {
            s_ptrPrefixStack.push_back(&s_diagJSON);
        }

        // check that type of nlohmann::json::object
        if (s_ptrPrefixStack.back()->type() != nlohmann::json::value_t::object) {
            throw std::runtime_error("Cannot insert to non-object type");
        }

        if constexpr (std::is_same_v<T, std::wstring>) {
            (*s_ptrPrefixStack.back())[key] = ParallaxGenUtil::utf16toUTF8(value);
        } else {
            (*s_ptrPrefixStack.back())[key] = value;
        }
    }

    template <typename T> static void pushBack(const T& value)
    {
        const std::lock_guard<std::mutex> lock(s_diagJSONMutex);

        if (!s_enabled) {
            return;
        }

        // check that type is nlohmann::json::array
        if (s_ptrPrefixStack.back()->type() != nlohmann::json::value_t::array) {
            throw std::runtime_error("Cannot push back to non-array type");
        }

        if constexpr (std::is_same_v<T, std::wstring>) {
            s_ptrPrefixStack.back()->push_back(ParallaxGenUtil::utf16toUTF8(value));
        } else {
            s_ptrPrefixStack.back()->push_back(value);
        }
    }
};
