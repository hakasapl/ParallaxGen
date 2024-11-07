#include "ParallaxGenWarnings.hpp"

#include <spdlog/spdlog.h>

using namespace std;

// statics
ParallaxGenDirectory *ParallaxGenWarnings::PGD = nullptr;
ParallaxGenConfig *ParallaxGenWarnings::PGC = nullptr;

unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::MismatchWarnTracker;
mutex ParallaxGenWarnings::MismatchWarnTrackerMutex;
unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::MismatchWarnDebugTracker;
mutex ParallaxGenWarnings::MismatchWarnDebugTrackerMutex;

unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::MeshWarnTracker;
mutex ParallaxGenWarnings::MeshWarnTrackerMutex;
unordered_set<pair<wstring, wstring>, ParallaxGenWarnings::PairHash> ParallaxGenWarnings::MeshWarnDebugTracker;
mutex ParallaxGenWarnings::MeshWarnDebugTrackerMutex;

void ParallaxGenWarnings::init(ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC) {
  ParallaxGenWarnings::PGD = PGD;
  ParallaxGenWarnings::PGC = PGC;

  MismatchWarnTracker.clear();
  MismatchWarnDebugTracker.clear();
  MeshWarnTracker.clear();
  MeshWarnDebugTracker.clear();
}

void ParallaxGenWarnings::mismatchWarn(const wstring &MatchedPath, const wstring &BaseTex) {
  // construct key
  auto MatchedPathMod = PGD->getMod(MatchedPath);
  auto BaseTexMod = PGD->getMod(BaseTex);

  if (MatchedPathMod.empty() || BaseTexMod.empty()) {
    return;
  }

  if (MatchedPathMod == BaseTexMod) {
    return;
  }

  auto Key = make_pair(MatchedPathMod, BaseTexMod);

  // Issue debug log
  {
    auto KeyDebug = make_pair(MatchedPath, BaseTex);
    const lock_guard<mutex> Lock(MismatchWarnDebugTrackerMutex);
    if (MismatchWarnDebugTracker.find(KeyDebug) != MismatchWarnDebugTracker.end()) {
      return;
    }

    MismatchWarnDebugTracker.insert(KeyDebug);

    spdlog::debug(L"[Tex Mismatch] Matched path {} from mod {} does not come from the same diffuse or normal {} from mod {}",
                  MatchedPath, MatchedPathMod, BaseTex, BaseTexMod);
  }

  // check if mod warning was already issued
  {
    const lock_guard<mutex> Lock(MismatchWarnTrackerMutex);
    if (MismatchWarnTracker.find(Key) != MismatchWarnTracker.end()) {
      return;
    }

    // add to tracker if not
    MismatchWarnTracker.insert(Key);
  }

  // log warning
  spdlog::warn(L"[Tex Mismatch] Mod \"{}\" assets were used with diffuse or normal from mod \"{}\". Please verify that "
               L"this is intended.",
               MatchedPathMod, BaseTexMod);
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

  if (PGC->getModPriority(NIFPathMod) < 0) {
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

    spdlog::debug(L"[Mesh Mismatch] Matched path {} from mod {} were used on mesh {} from mod {}", MatchedPath, MatchedPathMod, NIFPath,
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
  spdlog::warn(
      L"[Mesh Mismatch] Mod \"{}\" assets were used on meshes from mod \"{}\". Please verify that this is intended.",
      MatchedPathMod, NIFPathMod);
}
