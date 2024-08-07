#include "patchers/PatcherComplexMaterial.hpp"

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "ParallaxGenConfig.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

PatcherComplexMaterial::PatcherComplexMaterial(filesystem::path NIFPath, nifly::NifFile *NIF, vector<int> SlotSearch,
                                               vector<wstring> DynCubemapBlocklist, ParallaxGenConfig *PGC,
                                               ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D)
    : NIFPath(std::move(NIFPath)), NIF(NIF), SlotSearch(std::move(SlotSearch)),
      DynCubemapBlocklist(std::move(DynCubemapBlocklist)), PGD(PGD), PGC(PGC), PGD3D(PGD3D) {}

auto PatcherComplexMaterial::shouldApply(NiShape *NIFShape, const array<string, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                         bool &EnableResult, bool &EnableDynCubemaps,
                                         string &MatchedPath) const -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);
  auto *NIFShader = NIF->GetShader(NIFShape);

  EnableResult = true; // Start with default true

  // Check if complex material file exists
  for (int Slot : SlotSearch) {
    string FoundMatch = NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureSlots::EnvMask, PGC, PGD).string();
    if (!FoundMatch.empty()) {
      // found complex material map
      MatchedPath = FoundMatch;
      break;
    }
  }

  if (MatchedPath.empty()) {
    // no complex material map
    EnableResult = false;
    return Result;
  }

  // Get NIFShader type
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_ENVMAP && NIFShaderType != BSLSP_PARALLAX) {
    spdlog::trace(L"Rejecting shape {}: Incorrect NIFShader type", ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // verify that maps match each other
  string DiffuseMap;
  NIF->GetTextureSlot(NIFShape, DiffuseMap, 0);
  if (!DiffuseMap.empty() && !PGD->isFile(DiffuseMap)) {
    // no Diffuse map
    spdlog::trace("Rejecting shape {}: Diffuse map missing: {}", ShapeBlockID, DiffuseMap);
    EnableResult = false;
    return Result;
  }

  bool SameAspect = false;
  ParallaxGenTask::updatePGResult(Result, PGD3D->checkIfAspectRatioMatches(DiffuseMap, MatchedPath, SameAspect),
                                  ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
  if (!SameAspect) {
    spdlog::trace(L"Rejecting shape {} in NIF file {}: Complex material map does not "
                  L"match Diffuse map",
                  ShapeBlockID, NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // Determine if dynamic cubemaps should be set
  EnableDynCubemaps = !(ParallaxGenDirectory::checkGlob(NIFPath.wstring(), DynCubemapBlocklist) ||
                        ParallaxGenDirectory::checkGlob(stringToWstring(MatchedPath), DynCubemapBlocklist));

  return Result;
}

auto PatcherComplexMaterial::applyPatch(NiShape *NIFShape, const string &MatchedPath, const bool &ApplyDynCubemaps,
                                        bool &NIFModified) const -> ParallaxGenTask::PGResult {
  // enable complex material on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Set NIFShader type to env map
  NIFUtil::setShaderType(NIFShader, BSLSP_ENVMAP, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  // Set complex material texture
  string NewParallax;
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Parallax, NewParallax, NIFModified);
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::EnvMask, MatchedPath, NIFModified);

  // Dynamic cubemaps (if enabled)
  if (ApplyDynCubemaps) {
    string NewCubemap = ParallaxGenDirectory::getDefaultCubemapPath().string();
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Cubemap, NewCubemap, NIFModified);
  }

  return Result;
}
