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
  static ParallaxGenDirectory *PGD;
  static ParallaxGenConfig *PGC;
  static ParallaxGenD3D *PGD3D;

  static std::unordered_set<std::wstring> DynCubemapBlocklist; // NOLINT
  static bool DisableMLP;                                      // NOLINT

public:
  static auto loadStatics(const std::unordered_set<std::wstring> &DynCubemapBlocklist, const bool &DisableMLP,
                          ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D) -> void;

  PatcherComplexMaterial(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  // check if complex material should be enabled on shape
  auto shouldApply(nifly::NiShape &NIFShape, std::vector<NIFUtil::PatcherMatch> &Matches) const -> bool;
  static auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots, std::vector<NIFUtil::PatcherMatch> &Matches) -> bool;

  // enables complex material on a shape in a NIF
  auto applyPatch(nifly::NiShape &NIFShape, const NIFUtil::PatcherMatch &Match,
                  bool &NIFModified, std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) const -> ParallaxGenTask::PGResult;
  static auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots, const NIFUtil::PatcherMatch &Match, const std::wstring &NIFName) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;
};
