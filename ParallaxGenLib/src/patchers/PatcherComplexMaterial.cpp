#include "patchers/PatcherComplexMaterial.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

// Statics
std::unordered_set<wstring> PatcherComplexMaterial::DynCubemapBlocklist; // NOLINT

ParallaxGenDirectory *PatcherComplexMaterial::PGD;
ParallaxGenConfig *PatcherComplexMaterial::PGC;
ParallaxGenD3D *PatcherComplexMaterial::PGD3D;
bool PatcherComplexMaterial::DisableMLP; // NOLINT

auto PatcherComplexMaterial::loadStatics(const unordered_set<wstring> &DynCubemapBlocklist, const bool &DisableMLP,
                                         ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC,
                                         ParallaxGenD3D *PGD3D) -> void {
  PatcherComplexMaterial::DynCubemapBlocklist = DynCubemapBlocklist;
  PatcherComplexMaterial::DisableMLP = DisableMLP;
  PatcherComplexMaterial::PGD = PGD;
  PatcherComplexMaterial::PGC = PGC;
  PatcherComplexMaterial::PGD3D = PGD3D;
}

PatcherComplexMaterial::PatcherComplexMaterial(filesystem::path NIFPath, nifly::NifFile *NIF)
    : NIFPath(std::move(NIFPath)), NIF(NIF) {}

auto PatcherComplexMaterial::shouldApply(NiShape *NIFShape, const array<wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                         const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         bool &EnableResult,
                                         vector<wstring> &MatchedPathes) const -> ParallaxGenTask::PGResult {

  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);
  spdlog::trace(L"NIF: {} | Shape: {} | CM | Starting checking", NIFPath.wstring(), ShapeBlockID);

  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  EnableResult = true; // Start with default true

  if (shouldApplySlots(SearchPrefixes, OldSlots, MatchedPathes)) {
    for (const auto &MatchedPath : MatchedPathes) {
      spdlog::trace(L"NIF: {} | Shape: {} | CM | Found CM map: {}", NIFPath.wstring(), ShapeBlockID, MatchedPath);
    }
  } else {
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

  spdlog::trace(L"NIF: {} | Shape: {} | CM | Shape Accepted", NIFPath.wstring(), ShapeBlockID);
  return Result;
}

auto PatcherComplexMaterial::shouldApplySlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                              const array<wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                              std::vector<wstring> &MatchedPathes) -> bool {
  static const auto *CMBaseMap = &PGD->getTextureMap(NIFUtil::TextureSlots::ENVMASK);

  // Check if complex material file exists
  static const vector<int> SlotSearch = {1, 0}; // Diffuse first, then normal
  filesystem::path BaseMap;
  vector<NIFUtil::PGTexture> FoundMatches;
  for (int Slot : SlotSearch) {
    BaseMap = OldSlots[Slot];
    if (BaseMap.empty() || !PGD->isFile(BaseMap)) {
      continue;
    }

    FoundMatches.clear();
    FoundMatches = NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureType::COMPLEXMATERIAL, *CMBaseMap);

    if (!FoundMatches.empty()) {
      break;
    }
  }

  for (const auto &Match : FoundMatches) {
    bool SameAspect = false;
    PGD3D->checkIfAspectRatioMatches(BaseMap, Match.Path, SameAspect);
    if (SameAspect) {
      MatchedPathes.push_back(Match.Path);
    }
  }

  return !MatchedPathes.empty();
}

auto PatcherComplexMaterial::applyPatch(NiShape *NIFShape, const wstring &MatchedPath,
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
  const string NewParallax;
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::PARALLAX, NewParallax, NIFModified);
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::ENVMASK, MatchedPath, NIFModified);

  // Dynamic cubemaps (if enabled)
  bool EnableDynCubemaps = !(ParallaxGenDirectory::checkGlobMatchInSet(NIFPath, DynCubemapBlocklist) ||
                             ParallaxGenDirectory::checkGlobMatchInSet(MatchedPath, DynCubemapBlocklist));
  if (EnableDynCubemaps) {
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::CUBEMAP,
                            L"textures\\cubemaps\\dynamic1pxcubemap_black.dds", NIFModified);
  }

  return Result;
}

auto PatcherComplexMaterial::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const std::wstring &MatchedPath,
                                             const wstring &NIFName) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  // TODO disable-mlp needs to work here too

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = L"";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = MatchedPath;

  bool EnableDynCubemaps = !(ParallaxGenDirectory::checkGlobMatchInSet(NIFName, DynCubemapBlocklist) ||
                             ParallaxGenDirectory::checkGlobMatchInSet(MatchedPath, DynCubemapBlocklist));
  if (EnableDynCubemaps) {
    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)] = L"textures\\cubemaps\\dynamic1pxcubemap_black.dds";
  }

  return NewSlots;
}
