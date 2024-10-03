#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <shlwapi.h>
#include <string>
#include <unordered_set>
#include <winnt.h>

#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"

class PatcherComplexMaterial {
private:
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;
  ParallaxGenDirectory *PGD;
  ParallaxGenConfig *PGC;
  ParallaxGenD3D *PGD3D;

  static std::unordered_set<std::wstring> DynCubemapBlocklist; // NOLINT
  static bool DisableMLP; // NOLINT

public:
  static auto loadStatics(const std::unordered_set<std::wstring> &DynCubemapBlocklist, const bool &DisableMLP) -> void;

  PatcherComplexMaterial(std::filesystem::path NIFPath, nifly::NifFile *NIF, ParallaxGenConfig *PGC,
                         ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D);

  // check if complex material should be enabled on shape
  auto shouldApply(nifly::NiShape *NIFShape, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                   bool &EnableResult, bool &EnableDynCubemaps,
                   std::wstring &MatchedPath) const -> ParallaxGenTask::PGResult;

  // enables complex material on a shape in a NIF
  auto applyPatch(nifly::NiShape *NIFShape, const std::wstring &MatchedPath, const bool &ApplyDynCubemaps,
                  bool &NIFModified) const -> ParallaxGenTask::PGResult;
};
