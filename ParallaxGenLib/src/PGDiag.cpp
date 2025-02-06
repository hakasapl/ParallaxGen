#include "PGDiag.hpp"

#include "ParallaxGenUtil.hpp"
#include <mutex>

using namespace std;

thread_local std::vector<nlohmann::json*> PGDiag::s_ptrPrefixStack;

// ScopedPrefix class implementation
PGDiag::Prefix::Prefix(const wstring& prefix, const nlohmann::json::value_t& type)
    : Prefix(ParallaxGenUtil::utf16toUTF8(prefix), type)
{
}

PGDiag::Prefix::Prefix(const string& prefix, const nlohmann::json::value_t& type)
{
    const lock_guard<mutex> lock(s_diagJSONMutex);

    if (!s_enabled) {
        return;
    }

    if (s_ptrPrefixStack.empty()) {
        s_ptrPrefixStack.push_back(&s_diagJSON);
    }

    // create new object with key prefixStr and type type
    if (!s_ptrPrefixStack.back()->contains(prefix)) {
        (*s_ptrPrefixStack.back())[prefix] = nlohmann::json(type);
    }

    s_ptrPrefixStack.push_back(&s_ptrPrefixStack.back()->at(prefix));

    // check that type of s_ptrDiagJSONPrefix[prefixStr] is type
    if (s_ptrPrefixStack.back()->type() != type) {
        throw runtime_error("Type mismatch");
    }
}

PGDiag::Prefix::~Prefix()
{
    const lock_guard<mutex> lock(s_diagJSONMutex);

    if (!s_enabled) {
        return;
    }

    if (!s_ptrPrefixStack.empty()) {
        // remove last element of s_ptrPrefixStack
        s_ptrPrefixStack.pop_back();
    }
}

void PGDiag::init()
{
    const lock_guard<mutex> lock(s_diagJSONMutex);

    s_diagJSON = nlohmann::json::object();
    s_enabled = true;
}

auto PGDiag::getJSON() -> nlohmann::json
{
    const lock_guard<mutex> lock(s_diagJSONMutex);

    if (!s_enabled) {
        return nlohmann::json::object();
    }

    return s_diagJSON;
}
