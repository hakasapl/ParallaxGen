#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <string>

#include "Defs.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"

class PatcherVanillaParallax {
private:
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;
  std::vector<int> SlotSearch;
  ParallaxGenDirectory *PGD;
  ParallaxGenConfig *PGC;
  ParallaxGenD3D *PGD3D;

  bool HasAttachedHavok = false;

public:
  PatcherVanillaParallax(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::vector<int> SlotSearch,
                         ParallaxGenConfig *PGC, ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D);

  // check if vanilla parallax should be enabled on shape
  auto shouldEnableParallax(nifly::NiShape *NIFShape, const std::array<std::string, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                            bool &EnableResult, std::string &MatchedPath) const -> ParallaxGenTask::PGResult;

  // enables parallax on a shape in a NIF
  auto enableParallaxOnShape(nifly::NiShape *NIFShape, const std::string &MatchedPath,
                             bool &NIFModified) -> ParallaxGenTask::PGResult;
};
