#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <string>

#include "NIFUtil.hpp"
#include "Patchers/PatcherShader.hpp"

class PatcherVanillaParallax : public PatcherShader {
private:
  bool HasAttachedHavok = false;

public:
  PatcherVanillaParallax(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  // check if vanilla parallax should be enabled on shape
  auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool override;
  auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                   std::vector<PatcherMatch> &Matches) -> bool override;

  // enables parallax on a shape in a NIF
  auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                  bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;
  auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                       const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  auto applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
      -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;
};
