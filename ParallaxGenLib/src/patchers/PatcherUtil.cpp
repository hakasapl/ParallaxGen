#include "patchers/PatcherUtil.hpp"

#include "Logger.hpp"

using namespace std;

auto PatcherUtil::getWinningMatch(const vector<ShaderPatcherMatch> &Matches, const filesystem::path &NIFPath,
                                  const unordered_map<wstring, int> *ModPriority) -> ShaderPatcherMatch {
  // Find winning mod
  int MaxPriority = -1;
  auto WinningShaderMatch = PatcherUtil::ShaderPatcherMatch();

  // get mesh file priority from map
  int MeshFilePriority = -1;
  if (ModPriority != nullptr && ModPriority->find(NIFPath) != ModPriority->end()) {
    MeshFilePriority = ModPriority->at(NIFPath);
  }

  for (const auto &Match : Matches) {
    Logger::Prefix PrefixMod(Match.Mod);
    Logger::trace(L"Checking mod");

    int CurPriority = -1;
    if (ModPriority != nullptr && ModPriority->find(Match.Mod) != ModPriority->end()) {
      CurPriority = ModPriority->at(Match.Mod);
    }

    if (CurPriority < MeshFilePriority && CurPriority != -1 && MeshFilePriority != -1) {
      // skip mods with lower priority than mesh file
      Logger::trace(L"Rejecting: Mod has lower priority than mesh file");
      continue;
    }

    if (CurPriority < MaxPriority) {
      // skip mods with lower priority than current winner
      Logger::trace(L"Rejecting: Mod has lower priority than current winner");
      continue;
    }

    Logger::trace(L"Mod accepted");
    MaxPriority = CurPriority;
    WinningShaderMatch = Match;
  }

  Logger::trace(L"Winning mod: {}", WinningShaderMatch.Mod);
  return WinningShaderMatch;
}

auto PatcherUtil::applyTransformIfNeeded(const ShaderPatcherMatch &Match,
                                         const PatcherObjectSet &Patchers) -> ShaderPatcherMatch {
  auto TransformedMatch = Match;

  // Transform if required
  if (Match.ShaderTransformTo != NIFUtil::ShapeShader::NONE) {
    // Find transform object
    auto *const Transform = Patchers.ShaderTransformPatchers.at(Match.Shader).at(Match.ShaderTransformTo).get();

    // Transform Shader
    if (Transform->transform(Match.Match, TransformedMatch.Match)) {
      // Reset Transform
      TransformedMatch.Shader = Match.ShaderTransformTo;
    }

    TransformedMatch.ShaderTransformTo = NIFUtil::ShapeShader::NONE;
  }

  return TransformedMatch;
}
