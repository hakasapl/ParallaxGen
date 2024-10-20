#include "patchers/PatcherVanillaParallax.hpp"

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

// Statics
ModManagerDirectory *PatcherVanillaParallax::MMD;
ParallaxGenDirectory *PatcherVanillaParallax::PGD;
ParallaxGenConfig *PatcherVanillaParallax::PGC;
ParallaxGenD3D *PatcherVanillaParallax::PGD3D;

auto PatcherVanillaParallax::loadStatics(ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D, ModManagerDirectory *MMD) -> void {
  PatcherVanillaParallax::PGD = PGD;
  PatcherVanillaParallax::PGC = PGC;
  PatcherVanillaParallax::PGD3D = PGD3D;
  PatcherVanillaParallax::MMD = MMD;
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
                                         bool &EnableResult, wstring &MatchedPath) const -> ParallaxGenTask::PGResult {
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
  if (shouldApplySlots(SearchPrefixes, OldSlots, MatchedPath)) {
    spdlog::trace(L"NIF: {} | Shape: {} | Parallax | Found parallax map: {}", NIFPath.wstring(), ShapeBlockID, MatchedPath);
  } else {
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

auto PatcherVanillaParallax::shouldApplySlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                              std::wstring &MatchedPath) -> bool {
  static const auto *HeightBaseMap = &PGD->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);

  // Check if vanilla parallax file exists
  static const vector<int> SlotSearch = {1, 0}; // Diffuse first, then normal
  for (int Slot : SlotSearch) {
    auto FoundMatches = NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureType::HEIGHT, *HeightBaseMap);

    // TODO don't repeat this code across patchers
    // Check the priorities of each match
    int MaxPriority = -1;
    vector<NIFUtil::PGTexture> MaxPriorityMatches;
    for (const auto &FoundMatch : FoundMatches) {
      auto CurMod = PGD->getMod(FoundMatch.Path);
      auto CurModPriority = MMD->getModPriority(CurMod);

      if (CurModPriority > MaxPriority) {
        MaxPriority = CurModPriority;
        // clear vector since there is higher priority
        MaxPriorityMatches.clear();
      }

      // Add to vector if found match
      if (CurModPriority == MaxPriority) {
        MaxPriorityMatches.push_back(FoundMatch);
      }
    }

    // From within the max priority meshes, prefer ones that already exist in the slot
    for (const auto &FoundMatch : MaxPriorityMatches) {
      if (boost::iequals(OldSlots[static_cast<int>(NIFUtil::TextureSlots::PARALLAX)], FoundMatch.Path.wstring())) {
        MatchedPath = FoundMatch.Path.wstring();
        break;
      }
    }

    // Default if nothing is preferred
    if (MatchedPath.empty() && !MaxPriorityMatches.empty()) {
      // If no match was found, just take the first one
      MatchedPath = MaxPriorityMatches[0].Path.wstring();
    }
  }

  return !MatchedPath.empty();
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

auto PatcherVanillaParallax::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const std::wstring &MatchedPath) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = MatchedPath;

  return NewSlots;
}
