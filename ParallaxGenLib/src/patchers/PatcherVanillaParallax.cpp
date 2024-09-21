#include "patchers/PatcherVanillaParallax.hpp"

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

PatcherVanillaParallax::PatcherVanillaParallax(filesystem::path NIFPath, nifly::NifFile *NIF,
                                               ParallaxGenConfig *PGC, ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D)
    : NIFPath(std::move(NIFPath)), NIF(NIF), PGD(PGD), PGC(PGC), PGD3D(PGD3D) {
  // Determine if NIF has attached havok animations
  vector<NiObject *> NIFBlockTree;
  NIF->GetTree(NIFBlockTree);

  for (NiObject *NIFBlock : NIFBlockTree) {
    if (boost::iequals(NIFBlock->GetBlockName(), "BSBehaviorGraphExtraData")) {
      HasAttachedHavok = true;
    }
  }
}

auto PatcherVanillaParallax::shouldApply(NiShape *NIFShape, const array<wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                         bool &EnableResult, wstring &MatchedPath) const -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);
  spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Starting checking", NIFPath.wstring(), ShapeBlockID);

  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  static const auto *HeightBaseMap = &PGD->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);

  EnableResult = true; // Start with default true

  // Check if nif has attached havok (Results in crashes for vanilla Parallax)
  if (HasAttachedHavok) {
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Attached havok animations", NIFPath.wstring(),
                  ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Check if vanilla parallax file exists
  static const vector<int> SlotSearch = {0, 1};  // Diffuse first, then normal
  for (int Slot : SlotSearch) {
    auto FoundMatch = NIFUtil::getTexMatch(SearchPrefixes[Slot], *HeightBaseMap).Path.wstring();
    if (!FoundMatch.empty()) {
      // found parallax map
      spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Found parallax map: {}", NIFPath.wstring(), ShapeBlockID, MatchedPath);
      MatchedPath = FoundMatch;
      break;
    }
  }

  if (MatchedPath.empty()) {
    // no parallax map
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | No parallax map found", NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // ignore skinned meshes, these don't support Parallax
  if (NIFShape->HasSkinInstance() || NIFShape->IsSkinned()) {
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Skinned mesh", NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Check for shader type
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_PARALLAX) {
    // don't overwrite existing NIFShaders
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Incorrect NIFShader type", NIFPath.wstring(),
                  ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Check if TruePBR is enabled
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01)) {
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: TruePBR enabled", NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // decals don't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DECAL) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DYNAMIC_DECAL)) {
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Decal shape", NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Mesh lighting doesn't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_SOFT_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_RIM_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING)) {
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Lighting on shape", NIFPath.wstring(),
                  ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // verify that maps match each other (this is somewhat expense so it happens last)
  string DiffuseMap;
  NIF->GetTextureSlot(NIFShape, DiffuseMap, static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE));
  if (DiffuseMap.empty() || !PGD->isFile(DiffuseMap)) {
    // no Diffuse map
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Diffuse map missing: {}", NIFPath.wstring(),
                  ShapeBlockID, strToWstr(DiffuseMap));
    EnableResult = false;
    return Result;
  }

  bool SameAspect = false;
  ParallaxGenTask::updatePGResult(Result, PGD3D->checkIfAspectRatioMatches(DiffuseMap, MatchedPath, SameAspect),
                                  ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
  if (!SameAspect) {
    spdlog::trace(
        L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Aspect ratio of diffuse and parallax map do not match",
        NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // All checks passed
  spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Accepted", NIFPath.wstring(), ShapeBlockID);
  return Result;
}

auto PatcherVanillaParallax::applyPatch(NiShape *NIFShape, const wstring &MatchedPath,
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
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::PARALLAX, MatchedPath, NIFModified);

  return Result;
}
