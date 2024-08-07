#include "patchers/PatcherVanillaParallax.hpp"

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

PatcherVanillaParallax::PatcherVanillaParallax(filesystem::path NIFPath, nifly::NifFile *NIF, vector<int> SlotSearch,
                                               ParallaxGenConfig *PGC, ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D)
    : NIFPath(std::move(NIFPath)), NIF(NIF), SlotSearch(std::move(SlotSearch)), PGD(PGD), PGC(PGC), PGD3D(PGD3D) {
  // Determine if NIF has attached havok animations
  vector<NiObject *> NIFBlockTree;
  NIF->GetTree(NIFBlockTree);

  for (NiObject *NIFBlock : NIFBlockTree) {
    if (boost::iequals(NIFBlock->GetBlockName(), "BSBehaviorGraphExtraData")) {
      HasAttachedHavok = true;
    }
  }
}

auto PatcherVanillaParallax::shouldApply(NiShape *NIFShape, const array<string, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                         bool &EnableResult, string &MatchedPath) const -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  EnableResult = true; // Start with default true

  // Check if complex material file exists
  for (int Slot : SlotSearch) {
    string FoundMatch =
        wstringToString(NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureSlots::Parallax, PGC, PGD));
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

  // Check if nif has attached havok (Results in crashes for vanilla Parallax)
  if (HasAttachedHavok) {
    spdlog::trace(L"Rejecting NIF file {} due to attached havok animations", NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // ignore skinned meshes, these don't support Parallax
  if (NIFShape->HasSkinInstance() || NIFShape->IsSkinned()) {
    spdlog::trace(L"Rejecting shape {}: Skinned mesh", ShapeBlockID, NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // Enable regular Parallax for this shape!
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_PARALLAX) {
    // don't overwrite existing NIFShaders
    spdlog::trace(L"Rejecting shape {} in NIF file {}: Incorrect NIFShader type", ShapeBlockID, NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // Check if TruePBR is enabled
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01)) {
    spdlog::trace(L"Rejecting shape {} in NIF file {}: TruePBR enabled", ShapeBlockID, NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // decals don't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DECAL) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DYNAMIC_DECAL)) {
    spdlog::trace(L"Rejecting shape {} in NIF file {}: Decal shape", ShapeBlockID, NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // Mesh lighting doesn't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_SOFT_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_RIM_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING)) {
    spdlog::trace(L"Rejecting shape {} in NIF file {}: Lighting on shape", ShapeBlockID, NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // verify that maps match each other (this is somewhat expense so it happens
  // last)
  string DiffuseMap;
  NIF->GetTextureSlot(NIFShape, DiffuseMap, static_cast<unsigned int>(NIFUtil::TextureSlots::Diffuse));
  if (!DiffuseMap.empty() && !PGD->isFile(DiffuseMap)) {
    // no Diffuse map
    spdlog::trace(L"Rejecting shape {}: Diffuse map missing: {}", ShapeBlockID, stringToWstring(DiffuseMap));
    EnableResult = false;
    return Result;
  }

  bool SameAspect = false;
  ParallaxGenTask::updatePGResult(Result, PGD3D->checkIfAspectRatioMatches(DiffuseMap, MatchedPath, SameAspect),
                                  ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
  if (!SameAspect) {
    spdlog::trace(L"Rejecting shape {} in NIF file {}: Height map does not match Diffuse "
                  L"map",
                  ShapeBlockID, NIFPath.wstring());
    EnableResult = false;
    return Result;
  }

  // All checks passed
  return Result;
}

auto PatcherVanillaParallax::applyPatch(NiShape *NIFShape, const string &MatchedPath,
                                        bool &NIFModified) -> ParallaxGenTask::PGResult {
  // enable Parallax on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Set NIFShader type to Parallax
  NIFUtil::setShaderType(NIFShader, BSLSP_PARALLAX, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  // Set vertex colors for shape
  if (!NIFShape->HasVertexColors()) {
    NIFShape->SetVertexColors(true);
    NIFModified = true;
  }
  // Set vertex colors for NIFShader
  if (!NIFShader->HasVertexColors()) {
    NIFShader->SetVertexColors(true);
    NIFModified = true;
  }
  // Set Parallax heightmap texture
  string NewHeightMap = MatchedPath; // NOLINT
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Parallax, NewHeightMap, NIFModified);

  return Result;
}
