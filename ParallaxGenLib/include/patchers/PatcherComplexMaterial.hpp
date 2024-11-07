#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <shlwapi.h>
#include <string>
#include <unordered_set>
#include <winnt.h>

#include "NIFUtil.hpp"
#include "Patchers/PatcherShader.hpp"

class PatcherComplexMaterial : public PatcherShader {
private:
  static std::unordered_set<std::wstring> DynCubemapBlocklist;
  static bool DisableMLP;

public:
  PatcherComplexMaterial(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  static void loadStatics(const bool &DisableMLP, const std::unordered_set<std::wstring> &DynCubemapBlocklist);

  // check if complex material should be enabled on shape
  auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool override;
  auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                   std::vector<PatcherMatch> &Matches) -> bool override;

  // enables complex material on a shape in a NIF
  auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                  bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;
  auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                       const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  auto applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
      -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;
};
