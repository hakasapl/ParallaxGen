#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <string>

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
  ParallaxGenConfig *PGC;
  ParallaxGenD3D *PGD3D;

  bool HasAttachedHavok = false;

public:
  static auto loadStatics(ParallaxGenDirectory *PGD) -> void;

  PatcherVanillaParallax(std::filesystem::path NIFPath, nifly::NifFile *NIF,
                         ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D);

  // check if vanilla parallax should be enabled on shape
  auto shouldApply(nifly::NiShape *NIFShape, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                   bool &EnableResult, std::wstring &MatchedPath) const -> ParallaxGenTask::PGResult;

  static auto shouldApplySlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                        std::wstring &MatchedPath) -> bool;

  // enables parallax on a shape in a NIF
  auto applyPatch(nifly::NiShape *NIFShape, const std::wstring &MatchedPath,
                  bool &NIFModified) -> ParallaxGenTask::PGResult;

  static auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots, const std::wstring &MatchedPath) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;
};
