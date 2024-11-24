#include "ParallaxGenWarnings.hpp"

#include <spdlog/spdlog.h>

using namespace std;

// statics
ParallaxGenDirectory *ParallaxGenWarnings::PGD = nullptr;
const std::unordered_map<std::wstring, int> *ParallaxGenWarnings::ModPriority = nullptr;

map<wstring, set<wstring>> ParallaxGenWarnings::MismatchWarnTracker;
mutex ParallaxGenWarnings::MismatchWarnTrackerMutex;

std::map<std::wstring, std::set<std::pair<std::wstring,std::wstring>>> ParallaxGenWarnings::MismatchWarnDebugTracker;
mutex ParallaxGenWarnings::MismatchWarnDebugTrackerMutex;

unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::MeshWarnTracker;
mutex ParallaxGenWarnings::MeshWarnTrackerMutex;
unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::MeshWarnDebugTracker;
mutex ParallaxGenWarnings::MeshWarnDebugTrackerMutex;

void ParallaxGenWarnings::init(ParallaxGenDirectory *PGD, const unordered_map<wstring, int> *ModPriority) {
  ParallaxGenWarnings::PGD = PGD;
  ParallaxGenWarnings::ModPriority = ModPriority;

  MismatchWarnTracker.clear();
  MismatchWarnDebugTracker.clear();
  MeshWarnTracker.clear();
  MeshWarnDebugTracker.clear();
}

void ParallaxGenWarnings::mismatchWarn(const wstring &MatchedPath, const wstring &BaseTex) {
  // construct key
  auto MatchedPathMod = PGD->getMod(MatchedPath);
  auto BaseTexMod = PGD->getMod(BaseTex);

  if (BaseTexMod.empty())
    BaseTexMod = L"Vanilla Game";

  if (MatchedPathMod.empty() || BaseTexMod.empty()) {
    return;
  }

  if (MatchedPathMod == BaseTexMod) {
    return;
  }

   MismatchWarnDebugTracker[MatchedPathMod].insert(std::make_pair(MatchedPath,BaseTex));
   MismatchWarnTracker[MatchedPathMod].insert(BaseTexMod);
}

void ParallaxGenWarnings::printWarnings() {
  if (!MismatchWarnTracker.empty()) {
    spdlog::warn("");
    spdlog::warn("Potential Texture mismatches were found, there may be visual issues, Please verify each warning if "
                 "this is intended, address them and re-run ParallaxGen if needed.");
    spdlog::warn("See https://github.com/hakasapl/ParallaxGen/wiki/FAQ for further information");
    spdlog::warn("************************************************************");
  }
  for (auto MatchedMod : MismatchWarnTracker) {
    spdlog::warn(L"\"{}\" assets are used with:", MatchedMod.first);
    for (auto BaseMod : MatchedMod.second) {
      spdlog::warn(L"  - diffuse/normal textures from \"{}\"", BaseMod);
    }
    spdlog::warn("");
  }
  if (!MismatchWarnTracker.empty()) {
    spdlog::warn("************************************************************");
  }

  if (!MismatchWarnDebugTracker.empty())
  {
    spdlog::debug("Potential texture mismatches, textu");
  }
  for (auto MatchedMod : MismatchWarnDebugTracker)
  {
    spdlog::debug(L"Mod \"{}\":", MatchedMod.first);

    for (auto TexturePair : MatchedMod.second) {
      auto BaseTexMod = PGD->getMod(TexturePair.second);
      spdlog::debug(L"  - {} used with {} from \"{}\"",
                    TexturePair.first, TexturePair.second, BaseTexMod);
    }
  }
}

void ParallaxGenWarnings::meshWarn(const wstring &MatchedPath, const wstring &NIFPath) {
  // construct key
  auto MatchedPathMod = PGD->getMod(MatchedPath);
  auto NIFPathMod = PGD->getMod(NIFPath);

  if (MatchedPathMod.empty() || NIFPathMod.empty()) {
    return;
  }

  if (MatchedPathMod == NIFPathMod) {
    return;
  }

  int Priority = 0;
  if (ModPriority != nullptr && ModPriority->find(NIFPathMod) != ModPriority->end()) {
    Priority = ModPriority->at(NIFPathMod);
  }

  if (Priority < 0) {
    return;
  }

  auto Key = make_pair(MatchedPathMod, NIFPathMod);

  // Issue debug log
  {
    auto KeyDebug = make_pair(MatchedPath, NIFPath);
    const lock_guard<mutex> Lock(MeshWarnDebugTrackerMutex);
    if (MeshWarnDebugTracker.find(KeyDebug) != MeshWarnDebugTracker.end()) {
      return;
    }

    MeshWarnDebugTracker.insert(KeyDebug);

    spdlog::debug(L"[Potential Mesh Mismatch] Matched path {} from mod {} were used on mesh {} from mod {}", MatchedPath, MatchedPathMod, NIFPath,
                  NIFPathMod);
  }

  // check if warning was already issued
  {
    const lock_guard<mutex> Lock(MeshWarnTrackerMutex);
    if (MeshWarnTracker.find(Key) != MeshWarnTracker.end()) {
      return;
    }

    // add to tracker if not
    MeshWarnTracker.insert(Key);
  }

  // log warning
  //spdlog::warn(
  //    L"[Potential Mesh Mismatch] Mod \"{}\" assets were used on meshes from mod \"{}\". Please verify that this is intended.",
  //    MatchedPathMod, NIFPathMod);
}
