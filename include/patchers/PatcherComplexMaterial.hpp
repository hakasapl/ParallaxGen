#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <string>

#include "Defs.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"

class PatcherComplexMaterial {
private:
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;
  std::vector<int> SlotSearch;
  std::vector<std::wstring> DynCubemapBlocklist;
  ParallaxGenDirectory *PGD;
  ParallaxGenD3D *PGD3D;

public:
  PatcherComplexMaterial(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::vector<int> SlotSearch,
                         std::vector<std::wstring> DynCubemapBlocklist, ParallaxGenDirectory *PGD,
                         ParallaxGenD3D *PGD3D);

  // check if complex material should be enabled on shape
  auto shouldEnableComplexMaterial(nifly::NiShape *NIFShape,
                                   const std::array<std::string, NUM_TEXTURE_SLOTS> &SearchPrefixes, bool &EnableResult,
                                   bool &EnableDynCubemaps,
                                   std::string &MatchedPath) const -> ParallaxGenTask::PGResult;

  // enables complex material on a shape in a NIF
  auto enableComplexMaterialOnShape(nifly::NiShape *NIFShape, const std::string &MatchedPath,
                                    const bool &ApplyDynCubemaps, bool &NIFModified) const -> ParallaxGenTask::PGResult;
};
