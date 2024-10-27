#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <string>

#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"

class PatcherVanillaParallax {
private:
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;
  static ParallaxGenDirectory *PGD;
  static ParallaxGenConfig *PGC;
  static ParallaxGenD3D *PGD3D;

  bool HasAttachedHavok = false;

public:
  static auto loadStatics(ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D) -> void;

  PatcherVanillaParallax(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  // check if vanilla parallax should be enabled on shape
  auto shouldApply(nifly::NiShape *NIFShape, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                   bool &EnableResult, std::vector<std::wstring> &MatchedPathes, std::vector<std::unordered_set<NIFUtil::TextureSlots>> &MatchedFrom) const -> ParallaxGenTask::PGResult;

  static auto shouldApplySlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                        std::vector<std::wstring> &MatchedPathes, std::vector<std::unordered_set<NIFUtil::TextureSlots>> &MatchedFrom) -> bool;

  // enables parallax on a shape in a NIF
  auto applyPatch(nifly::NiShape *NIFShape, const std::wstring &MatchedPath,
                  bool &NIFModified, std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) -> ParallaxGenTask::PGResult;

  static auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots, const std::wstring &MatchedPath) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;
};
