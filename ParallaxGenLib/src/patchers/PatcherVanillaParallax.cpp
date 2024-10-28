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

auto PatcherVanillaParallax::loadStatics(ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC,
                                         ParallaxGenD3D *PGD3D) -> void {
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

auto PatcherVanillaParallax::shouldApply(NiShape &NIFShape, vector<NIFUtil::PatcherMatch> &Matches) const -> bool {
  // Prep
  spdlog::trace(L"Starting checking");

  auto *NIFShader = NIF->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Check if nif has attached havok (Results in crashes for vanilla Parallax)
  if (HasAttachedHavok) {
    spdlog::trace("Shape Rejected: Attached havok animations");
    return false;
  }

  // Get slots
  auto OldSlots = NIFUtil::getTextureSlots(NIF, &NIFShape);

  // Check if parallax map exists
  if (shouldApply(OldSlots, Matches)) {
    for (const auto &MatchedPath : Matches) {
      spdlog::trace(L"Found Parallax map: {}", MatchedPath.MatchedPath);
    }
  } else {
    spdlog::trace("No Parallax map found");
    return false;
  }

  // ignore skinned meshes, these don't support Parallax
  if (NIFShape.HasSkinInstance() || NIFShape.IsSkinned()) {
    spdlog::trace("Shape Rejected: Skinned mesh");
    return false;
  }

  // Check for shader type
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_PARALLAX && NIFShaderType != BSLSP_ENVMAP) {
    // don't overwrite existing NIFShaders
    spdlog::trace("Shape Rejected: Incorrect NIFShader type");
    return false;
  }

  // decals don't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DECAL) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DYNAMIC_DECAL)) {
    spdlog::trace("Shape Rejected: Shape has decal");
    return false;
  }

  // Mesh lighting doesn't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_SOFT_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_RIM_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING)) {
    spdlog::trace("Shape Rejected: Lighting on shape");
    return false;
  }

  // All checks passed
  spdlog::trace("Shape Accepted");
  return true;
}

auto PatcherVanillaParallax::shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         std::vector<NIFUtil::PatcherMatch> &Matches) -> bool {
  static const auto *HeightBaseMap = &PGD->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);

  Matches.clear();

  // Search prefixes
  const auto SearchPrefixes = NIFUtil::getSearchPrefixes(OldSlots);

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
      NIFUtil::PatcherMatch PatcherMatch;
      PatcherMatch.MatchedPath = Match.Path;
      PatcherMatch.MatchedFrom.insert(MatchedFromSlot);
      Matches.push_back(PatcherMatch);
    }
  }

  return !Matches.empty();
}

auto PatcherVanillaParallax::applyPatch(nifly::NiShape &NIFShape, const NIFUtil::PatcherMatch &Match, bool &NIFModified,
                                        std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots)
    -> ParallaxGenTask::PGResult {
  // enable Parallax on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Set NIFShader type to Parallax
  NIFUtil::setShaderType(NIFShader, BSLSP_PARALLAX, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  // Set vertex colors for shape
  if (!NIFShape.HasVertexColors()) {
    NIFShape.SetVertexColors(true);
    NIFModified = true;
  }
  // Set vertex colors for NIFShader
  if (!NIFShader->HasVertexColors()) {
    NIFShader->SetVertexColors(true);
    NIFModified = true;
  }

  NewSlots = applyPatchSlots(NIFUtil::getTextureSlots(NIF, &NIFShape), Match);
  NIFUtil::setTextureSlots(NIF, &NIFShape, NewSlots, NIFModified);

  return Result;
}

auto PatcherVanillaParallax::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const NIFUtil::PatcherMatch &Match)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = Match.MatchedPath;

  return NewSlots;
}
