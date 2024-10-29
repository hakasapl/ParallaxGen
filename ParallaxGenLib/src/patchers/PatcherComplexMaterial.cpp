#include "patchers/PatcherComplexMaterial.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

#include "NIFUtil.hpp"

using namespace std;

// Statics
std::unordered_set<wstring> PatcherComplexMaterial::DynCubemapBlocklist;
bool PatcherComplexMaterial::DisableMLP;

auto PatcherComplexMaterial::loadStatics(const bool &DisableMLP,
                                         const std::unordered_set<std::wstring> &DynCubemapBlocklist) -> void {
  PatcherComplexMaterial::DynCubemapBlocklist = DynCubemapBlocklist;
  PatcherComplexMaterial::DisableMLP = DisableMLP;
}

PatcherComplexMaterial::PatcherComplexMaterial(filesystem::path NIFPath, nifly::NifFile *NIF)
    : PatcherShader(std::move(NIFPath), NIF, "ComplexMaterial", NIFUtil::ShapeShader::COMPLEXMATERIAL) {}

auto PatcherComplexMaterial::shouldApply(NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool {
  // Prep
  spdlog::trace("Starting checking");

  auto *NIFShader = getNIF()->GetShader(&NIFShape);

  // Get slots
  auto OldSlots = NIFUtil::getTextureSlots(getNIF(), &NIFShape);

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
      (!NIFUtil::getTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::GLOW).empty() ||
       !NIFUtil::getTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::TINT).empty() ||
       !NIFUtil::getTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::BACKLIGHT).empty())) {
    spdlog::trace(L"Shape Rejected: Texture defined in slots 3,7,or 8");
    return false;
  }

  spdlog::trace("Shape Accepted");
  return true;
}

auto PatcherComplexMaterial::shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         std::vector<PatcherMatch> &Matches) -> bool {
  static const auto CMBaseMap = getSlotLookupMap(NIFUtil::TextureSlots::ENVMASK);

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
    if (BaseMap.empty() || !isFile(BaseMap)) {
      continue;
    }

    FoundMatches.clear();
    FoundMatches = NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureType::COMPLEXMATERIAL, CMBaseMap);

    if (!FoundMatches.empty()) {
      // TODO should we be trying diffuse after normal too and present all options?
      MatchedFromSlot = static_cast<NIFUtil::TextureSlots>(Slot);
      break;
    }
  }

  for (const auto &Match : FoundMatches) {
    if (isSameAspectRatio(BaseMap, Match.Path)) {
      PatcherMatch PatcherMatch;
      PatcherMatch.MatchedPath = Match.Path;
      PatcherMatch.MatchedFrom.insert(MatchedFromSlot);
      Matches.push_back(PatcherMatch);
    }
  }

  return !Matches.empty();
}

auto PatcherComplexMaterial::applyPatch(NiShape &NIFShape, const PatcherMatch &Match,
                                        bool &NIFModified, bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  // Prep
  ShapeDeleted = false;
  auto *NIFShader = getNIF()->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Remove texture slots if disabling MLP
  if (DisableMLP && NIFShaderBSLSP->GetShaderType() == BSLSP_MULTILAYERPARALLAX) {
    NIFUtil::setTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::GLOW, "", NIFModified);
    NIFUtil::setTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::TINT, "", NIFModified);
    NIFUtil::setTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::BACKLIGHT, "", NIFModified);

    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, NIFModified);
  }

  // Set NIFShader type to env map
  NIFUtil::setShaderType(NIFShader, BSLSP_ENVMAP, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);

  auto NewSlots = applyPatchSlots(NIFUtil::getTextureSlots(getNIF(), &NIFShape), Match);
  NIFUtil::setTextureSlots(getNIF(), &NIFShape, NewSlots, NIFModified);

  return NewSlots;
}

auto PatcherComplexMaterial::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  const auto MatchedPath = Match.MatchedPath;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = L"";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = MatchedPath;

  bool EnableDynCubemaps = !(ParallaxGenDirectory::checkGlobMatchInSet(getNIFPath().wstring(), DynCubemapBlocklist) ||
                             ParallaxGenDirectory::checkGlobMatchInSet(MatchedPath, DynCubemapBlocklist));
  if (EnableDynCubemaps) {
    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)] = L"textures\\cubemaps\\dynamic1pxcubemap_black.dds";
  }

  return NewSlots;
}
