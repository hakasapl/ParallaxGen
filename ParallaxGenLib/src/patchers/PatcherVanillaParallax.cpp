#include "patchers/PatcherVanillaParallax.hpp"

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"

using namespace std;

// Statics
ParallaxGenDirectory *PatcherVanillaParallax::PGD;
ParallaxGenConfig *PatcherVanillaParallax::PGC;
ParallaxGenD3D *PatcherVanillaParallax::PGD3D;

auto PatcherVanillaParallax::loadStatics(ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D) -> void {
  PatcherVanillaParallax::PGD = PGD;
  PatcherVanillaParallax::PGC = PGC;
  PatcherVanillaParallax::PGD3D = PGD3D;
}

PatcherVanillaParallax::PatcherVanillaParallax(filesystem::path NIFPath, nifly::NifFile *NIF)
    : NIFPath(std::move(NIFPath)), NIF(NIF) {
  // Determine if NIF has attached havok animations
  vector<NiObject *> NIFBlockTree;
  NIF->GetTree(NIFBlockTree);

  for (NiObject *NIFBlock : NIFBlockTree) {
    if (boost::iequals(NIFBlock->GetBlockName(), "BSBehaviorGraphExtraData")) {
      HasAttachedHavok = true;
    }
  }
}

auto PatcherVanillaParallax::shouldApply(NiShape *NIFShape, const array<wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         bool &EnableResult, vector<wstring> &MatchedPathes, std::vector<std::unordered_set<NIFUtil::TextureSlots>> &MatchedFrom) const -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);
  spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Starting checking", NIFPath.wstring(), ShapeBlockID);

  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  EnableResult = true; // Start with default true

  // Check if nif has attached havok (Results in crashes for vanilla Parallax)
  if (HasAttachedHavok) {
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Attached havok animations", NIFPath.wstring(),
                  ShapeBlockID);
    EnableResult = false;
    return Result;
  }

  // Check if parallax map exists
  if (shouldApplySlots(SearchPrefixes, OldSlots, MatchedPathes, MatchedFrom)) {
    for (const auto &MatchedPath : MatchedPathes) {
      spdlog::trace(L"NIF: {} | Shape: {} | CM | Found Parallax map: {}", NIFPath.wstring(), ShapeBlockID, MatchedPath);
    }
  } else {
    spdlog::trace(L"NIF: {} | Shape: {} | CM | No Parallax map found", NIFPath.wstring(), ShapeBlockID);
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
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_PARALLAX && NIFShaderType != BSLSP_ENVMAP) {
    // don't overwrite existing NIFShaders
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Rejected: Incorrect NIFShader type", NIFPath.wstring(),
                  ShapeBlockID);
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

  // All checks passed
  spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Shape Accepted", NIFPath.wstring(), ShapeBlockID);
  return Result;
}

auto PatcherVanillaParallax::shouldApplySlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                              vector<wstring> &MatchedPathes, std::vector<std::unordered_set<NIFUtil::TextureSlots>> &MatchedFrom) -> bool {
  static const auto *HeightBaseMap = &PGD->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);

  // Check if parallax file exists
  static const vector<int> SlotSearch = {1, 0}; // Diffuse first, then normal
  filesystem::path BaseMap;
  vector<NIFUtil::PGTexture> FoundMatches;
  NIFUtil::TextureSlots MatchedFromSlot = NIFUtil::TextureSlots::NORMAL;
  for (int Slot : SlotSearch) {
    BaseMap = OldSlots[Slot];
    if (BaseMap.empty() || !PGD->isFile(BaseMap)) {
      continue;
    }

    FoundMatches.clear();
    FoundMatches = NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureType::HEIGHT, *HeightBaseMap);

    if (!FoundMatches.empty()) {
      // TODO should we be trying diffuse after normal too and present all options?
      MatchedFromSlot = static_cast<NIFUtil::TextureSlots>(Slot);
      break;
    }
  }

  // Check aspect ratio matches
  for (const auto &Match : FoundMatches) {
    bool SameAspect = false;
    PGD3D->checkIfAspectRatioMatches(BaseMap, Match.Path, SameAspect);
    if (SameAspect) {
      MatchedPathes.push_back(Match.Path);
      MatchedFrom.push_back({MatchedFromSlot});
    }
  }

  return !MatchedPathes.empty();
}

auto PatcherVanillaParallax::applyPatch(NiShape *NIFShape, const wstring &MatchedPath,
                                        bool &NIFModified, std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) -> ParallaxGenTask::PGResult {
  // enable Parallax on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Set NIFShader type to Parallax
  NIFUtil::setShaderType(NIFShader, BSLSP_PARALLAX, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);
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

  NewSlots = applyPatchSlots(NIFUtil::getTextureSlots(NIF, NIFShape), MatchedPath);
  NIFUtil::setTextureSlots(NIF, NIFShape, NewSlots, NIFModified);

  return Result;
}

auto PatcherVanillaParallax::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const std::wstring &MatchedPath) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = MatchedPath;

  return NewSlots;
}
