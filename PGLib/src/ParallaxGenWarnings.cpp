#include "ParallaxGenWarnings.hpp"

#include <mutex>
#include <spdlog/spdlog.h>

using namespace std;

// statics
ParallaxGenDirectory* ParallaxGenWarnings::s_pgd = nullptr;
const std::unordered_map<std::wstring, int>* ParallaxGenWarnings::s_modPriority = nullptr;

unordered_map<wstring, unordered_set<wstring>> ParallaxGenWarnings::s_mismatchWarnTracker;
mutex ParallaxGenWarnings::s_mismatchWarnTrackerMutex;

unordered_map<std::wstring, unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash>>
    ParallaxGenWarnings::s_mismatchWarnDebugTracker;
mutex ParallaxGenWarnings::s_mismatchWarnDebugTrackerMutex;

unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::s_meshWarnTracker;
mutex ParallaxGenWarnings::s_meshWarnTrackerMutex;
unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::s_meshWarnDebugTracker;
mutex ParallaxGenWarnings::s_meshWarnDebugTrackerMutex;

void ParallaxGenWarnings::init(ParallaxGenDirectory* pgd, const unordered_map<wstring, int>* modPriority)
{
    ParallaxGenWarnings::s_pgd = pgd;
    ParallaxGenWarnings::s_modPriority = modPriority;

    s_mismatchWarnTracker.clear();
    s_mismatchWarnDebugTracker.clear();
    s_meshWarnTracker.clear();
    s_meshWarnDebugTracker.clear();
}

void ParallaxGenWarnings::mismatchWarn(const wstring& matchedPath, const wstring& baseTex)
{
    // construct key
    auto matchedPathMod = s_pgd->getMod(matchedPath);
    auto baseTexMod = s_pgd->getMod(baseTex);

    if (baseTexMod.empty()) {
        baseTexMod = L"Vanilla Game";
    }

    if (matchedPathMod.empty() || baseTexMod.empty()) {
        return;
    }

    if (matchedPathMod == baseTexMod) {
        return;
    }

    {
        const lock_guard<mutex> lock(s_mismatchWarnDebugTrackerMutex);
        s_mismatchWarnDebugTracker[matchedPathMod].insert(std::make_pair(matchedPath, baseTex));
    }

    {
        const lock_guard<mutex> lock2(s_mismatchWarnTrackerMutex);
        s_mismatchWarnTracker[matchedPathMod].insert(baseTexMod);
    }
}

void ParallaxGenWarnings::printWarnings()
{
    if (!s_mismatchWarnTracker.empty()) {
        spdlog::warn(
            "Potential Texture mismatches were found, there may be visual issues, Please verify for each warning if "
            "this is intended, address them and re-run ParallaxGen if needed.");
        spdlog::warn("See https://github.com/hakasapl/ParallaxGen/wiki/FAQ for further information");
        spdlog::warn("************************************************************");
    }
    for (const auto& matchedMod : s_mismatchWarnTracker) {
        spdlog::warn(L"\"{}\" assets are used with:", matchedMod.first);
        for (auto baseMod : matchedMod.second) {
            spdlog::warn(L"  - diffuse/normal textures from \"{}\"", baseMod);
        }
        spdlog::warn("");
    }
    if (!s_mismatchWarnTracker.empty()) {
        spdlog::warn("************************************************************");
    }

    if (!s_mismatchWarnDebugTracker.empty()) {
        spdlog::debug("Potential texture mismatches:");
    }
    for (auto& matchedMod : s_mismatchWarnDebugTracker) {
        spdlog::debug(L"Mod \"{}\":", matchedMod.first);

        for (auto texturePair : matchedMod.second) {
            auto baseTexMod = s_pgd->getMod(texturePair.second);
            spdlog::debug(L"  - {} used with {} from \"{}\"", texturePair.first, texturePair.second, baseTexMod);
        }
    }
}

void ParallaxGenWarnings::meshWarn(const wstring& matchedPath, const wstring& nifPath)
{
    // construct key
    auto matchedPathMod = s_pgd->getMod(matchedPath);
    auto nifPathMod = s_pgd->getMod(nifPath);

    if (matchedPathMod.empty() || nifPathMod.empty()) {
        return;
    }

    if (matchedPathMod == nifPathMod) {
        return;
    }

    int priority = 0;
    if (s_modPriority != nullptr && s_modPriority->find(nifPathMod) != s_modPriority->end()) {
        priority = s_modPriority->at(nifPathMod);
    }

    if (priority < 0) {
        return;
    }

    auto key = make_pair(matchedPathMod, nifPathMod);

    // Issue debug log
    {
        auto keyDebug = make_pair(matchedPath, nifPath);
        const lock_guard<mutex> lock(s_meshWarnDebugTrackerMutex);
        if (s_meshWarnDebugTracker.find(keyDebug) != s_meshWarnDebugTracker.end()) {
            return;
        }

        s_meshWarnDebugTracker.insert(keyDebug);

        spdlog::debug(L"[Potential Mesh Mismatch] Matched path {} from mod {} were used on mesh {} from mod {}",
            matchedPath, matchedPathMod, nifPath, nifPathMod);
    }

    // check if warning was already issued
    {
        const lock_guard<mutex> lock(s_meshWarnTrackerMutex);
        if (s_meshWarnTracker.find(key) != s_meshWarnTracker.end()) {
            return;
        }

        // add to tracker if not
        s_meshWarnTracker.insert(key);
    }
}
