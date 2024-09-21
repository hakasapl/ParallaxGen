#include "patchers/PatcherComplexMaterial.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

// Statics
std::unordered_set<wstring> PatcherComplexMaterial::DynCubemapBlocklist; // NOLINT
bool PatcherComplexMaterial::DisableMLP;

auto PatcherComplexMaterial::loadStatics(const unordered_set<wstring> &DynCubemapBlocklist,
                                         const bool &DisableMLP) -> void {
  PatcherComplexMaterial::DynCubemapBlocklist = DynCubemapBlocklist;
  PatcherComplexMaterial::DisableMLP = DisableMLP;
}

PatcherComplexMaterial::PatcherComplexMaterial(filesystem::path NIFPath, nifly::NifFile *NIF, ParallaxGenConfig *PGC,
                                               ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D)
    : NIFPath(std::move(NIFPath)), NIF(NIF), PGD(PGD), PGC(PGC), PGD3D(PGD3D) {}

auto PatcherComplexMaterial::shouldApply(NiShape *NIFShape, const array<wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                         bool &EnableResult, bool &EnableDynCubemaps,
                                         wstring &MatchedPath) const -> ParallaxGenTask::PGResult {

  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);
  spdlog::trace(L"NIF: {} | Shape: {} | CM | Starting checking", NIFPath.wstring(), ShapeBlockID);

  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  static const auto *CMBaseMap = &PGD->getTextureMap(NIFUtil::TextureSlots::ENVMASK);

  EnableResult = true; // Start with default true

  // Check if complex material file exists
  static const vector<int> SlotSearch = {0, 1}; // Diffuse first, then normal
  for (int Slot : SlotSearch) {
    auto FoundMatch = NIFUtil::getTexMatch(SearchPrefixes[Slot], *CMBaseMap);
    if (!FoundMatch.Path.empty() && FoundMatch.Type == NIFUtil::TextureType::COMPLEXMATERIAL) {
      // found complex material map
      spdlog::trace(L"NIF: {} | Shape: {} | CM | Found CM map: {}", NIFPath.wstring(), ShapeBlockID, MatchedPath);
      MatchedPath = FoundMatch.Path.wstring();
      break;
    }
  }

  if (MatchedPath.empty()) {
    // no complex material map
    spdlog::trace(L"NIF: {} | Shape: {} | CM | No CM map found", NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Get NIFShader type
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_ENVMAP && NIFShaderType != BSLSP_PARALLAX &&
      (NIFShaderType != BSLSP_MULTILAYERPARALLAX || !DisableMLP)) {
    spdlog::trace(L"NIF: {} | Shape: {} | CM | Shape Rejected: Incorrect NIFShader type", NIFPath.wstring(),
                  ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Check if TruePBR is enabled
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01)) {
    spdlog::trace(L"NIF: {} | Shape: {} | CM | Shape Rejected: TruePBR enabled", NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // check to make sure there are textures defined in slot 3 or 8
  if (NIFShaderType != BSLSP_MULTILAYERPARALLAX &&
      (!NIFUtil::getTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::GLOW).empty() ||
       !NIFUtil::getTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::TINT).empty() ||
       !NIFUtil::getTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::BACKLIGHT).empty())) {
    spdlog::trace(L"NIF: {} | Shape: {} | CM | Shape Rejected: Texture defined in slots 3,7,or 8", NIFPath.wstring(),
                  ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // verify that maps match each other
  string DiffuseMap;
  NIF->GetTextureSlot(NIFShape, DiffuseMap, 0);
  if (DiffuseMap.empty() || !PGD->isFile(DiffuseMap)) {
    // no Diffuse map
    spdlog::trace(L"NIF: {} | Shape: {} | CM | Shape Rejected: Diffuse map missing: {}", NIFPath.wstring(),
                  ShapeBlockID, strToWstr(DiffuseMap));
    EnableResult = false;
    return Result;
  }

  bool SameAspect = false;
  ParallaxGenTask::updatePGResult(Result, PGD3D->checkIfAspectRatioMatches(DiffuseMap, MatchedPath, SameAspect),
                                  ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
  if (!SameAspect) {
    spdlog::trace(L"NIF: {} | Shape: {} | CM | Shape Rejected: Aspect ratio of diffuse and CM map do not match",
                  NIFPath.wstring(), ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Determine if dynamic cubemaps should be set
  EnableDynCubemaps = !(ParallaxGenDirectory::checkGlobMatchInSet(NIFPath.wstring(), DynCubemapBlocklist) ||
                        ParallaxGenDirectory::checkGlobMatchInSet(MatchedPath, DynCubemapBlocklist));

  spdlog::trace(L"NIF: {} | Shape: {} | CM | Shape Accepted", NIFPath.wstring(), ShapeBlockID);
  return Result;
}

auto PatcherComplexMaterial::applyPatch(NiShape *NIFShape, const wstring &MatchedPath, const bool &ApplyDynCubemaps,
                                        bool &NIFModified) const -> ParallaxGenTask::PGResult {
  // enable complex material on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Remove texture slots if disabling MLP
  if (DisableMLP && NIFShaderBSLSP->GetShaderType() == BSLSP_MULTILAYERPARALLAX) {
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::GLOW, "", NIFModified);
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::TINT, "", NIFModified);
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::BACKLIGHT, "", NIFModified);

    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, NIFModified);
  }

  // Set NIFShader type to env map
  NIFUtil::setShaderType(NIFShader, BSLSP_ENVMAP, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  // Set complex material texture
  string NewParallax;
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::PARALLAX, NewParallax, NIFModified);
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::ENVMASK, MatchedPath, NIFModified);

  // Dynamic cubemaps (if enabled)
  if (ApplyDynCubemaps) {
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::CUBEMAP,
                            "textures\\cubemaps\\dynamic1pxcubemap_black.dds", NIFModified);
  }

  return Result;
}
