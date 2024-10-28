#include "patchers/PatcherComplexMaterial.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"

using namespace std;

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

auto PatcherComplexMaterial::shouldApply(NiShape &NIFShape, std::vector<NIFUtil::PatcherMatch> &Matches) const -> bool {
  // Prep
  spdlog::trace("Starting checking");

  auto *NIFShader = NIF->GetShader(&NIFShape);

  // Get slots
  auto OldSlots = NIFUtil::getTextureSlots(NIF, &NIFShape);

  if (shouldApply(OldSlots, Matches)) {
    for (const auto &MatchedPath : Matches) {
      spdlog::trace(L"Found CM map: {}", MatchedPath.MatchedPath);
    }
  } else {
    spdlog::trace("No CM map found");
    return false;
  }

  // Get NIFShader type
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_ENVMAP && NIFShaderType != BSLSP_PARALLAX &&
      (NIFShaderType != BSLSP_MULTILAYERPARALLAX || !DisableMLP)) {
    spdlog::trace("Shape Rejected: Incorrect NIFShader type");
    return false;
  }

  // check to make sure there are textures defined in slot 3 or 8
  if (NIFShaderType != BSLSP_MULTILAYERPARALLAX &&
      (!NIFUtil::getTextureSlot(NIF, &NIFShape, NIFUtil::TextureSlots::GLOW).empty() ||
       !NIFUtil::getTextureSlot(NIF, &NIFShape, NIFUtil::TextureSlots::TINT).empty() ||
       !NIFUtil::getTextureSlot(NIF, &NIFShape, NIFUtil::TextureSlots::BACKLIGHT).empty())) {
    spdlog::trace(L"Shape Rejected: Texture defined in slots 3,7,or 8");
    return false;
  }

  spdlog::trace("Shape Accepted");
  return true;
}

auto PatcherComplexMaterial::shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         std::vector<NIFUtil::PatcherMatch> &Matches) -> bool {
  static const auto *CMBaseMap = &PGD->getTextureMap(NIFUtil::TextureSlots::ENVMASK);

  Matches.clear();

  // Search prefixes
  const auto SearchPrefixes = NIFUtil::getSearchPrefixes(OldSlots);

  // Check if complex material file exists
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
    FoundMatches = NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureType::COMPLEXMATERIAL, *CMBaseMap);

    if (!FoundMatches.empty()) {
      // TODO should we be trying diffuse after normal too and present all options?
      MatchedFromSlot = static_cast<NIFUtil::TextureSlots>(Slot);
      break;
    }
  }

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

auto PatcherComplexMaterial::applyPatch(NiShape &NIFShape, const NIFUtil::PatcherMatch &Match, bool &NIFModified,
                                        std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) const
    -> ParallaxGenTask::PGResult {
  // enable complex material on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Remove texture slots if disabling MLP
  if (DisableMLP && NIFShaderBSLSP->GetShaderType() == BSLSP_MULTILAYERPARALLAX) {
    NIFUtil::setTextureSlot(NIF, &NIFShape, NIFUtil::TextureSlots::GLOW, "", NIFModified);
    NIFUtil::setTextureSlot(NIF, &NIFShape, NIFUtil::TextureSlots::TINT, "", NIFModified);
    NIFUtil::setTextureSlot(NIF, &NIFShape, NIFUtil::TextureSlots::BACKLIGHT, "", NIFModified);

    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, NIFModified);
  }

  // Set NIFShader type to env map
  NIFUtil::setShaderType(NIFShader, BSLSP_ENVMAP, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);

  NewSlots = applyPatchSlots(NIFUtil::getTextureSlots(NIF, &NIFShape), Match, NIFPath.wstring());
  NIFUtil::setTextureSlots(NIF, &NIFShape, NewSlots, NIFModified);

  return Result;
}

auto PatcherComplexMaterial::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const NIFUtil::PatcherMatch &Match,
                                             const wstring &NIFName) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  const auto MatchedPath = Match.MatchedPath;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = L"";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = MatchedPath;

  bool EnableDynCubemaps = !(ParallaxGenDirectory::checkGlobMatchInSet(NIFName, DynCubemapBlocklist) ||
                             ParallaxGenDirectory::checkGlobMatchInSet(MatchedPath, DynCubemapBlocklist));
  if (EnableDynCubemaps) {
    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)] = L"textures\\cubemaps\\dynamic1pxcubemap_black.dds";
  }

  return NewSlots;
}
