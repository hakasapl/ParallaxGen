#include "patchers/PatcherComplexMaterial.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>

#include "Logger.hpp"
#include "NIFUtil.hpp"

using namespace std;

// Statics
std::vector<wstring> PatcherComplexMaterial::DynCubemapBlocklist;
bool PatcherComplexMaterial::DisableMLP;

auto PatcherComplexMaterial::loadStatics(const bool &DisableMLP,
                                         const std::vector<std::wstring> &DynCubemapBlocklist) -> void {
  PatcherComplexMaterial::DynCubemapBlocklist = DynCubemapBlocklist;
  PatcherComplexMaterial::DisableMLP = DisableMLP;
}

auto PatcherComplexMaterial::getFactory() -> PatcherShader::PatcherShaderFactory {
  return [](const filesystem::path &NIFPath, nifly::NifFile *NIF) -> unique_ptr<PatcherShader> {
    return make_unique<PatcherComplexMaterial>(NIFPath, NIF);
  };
}

auto PatcherComplexMaterial::getShaderType() -> NIFUtil::ShapeShader { return NIFUtil::ShapeShader::COMPLEXMATERIAL; }

PatcherComplexMaterial::PatcherComplexMaterial(filesystem::path NIFPath, nifly::NifFile *NIF)
    : PatcherShader(std::move(NIFPath), NIF, "ComplexMaterial") {}

auto PatcherComplexMaterial::canApply(NiShape &NIFShape) -> bool {
  // Prep
  Logger::trace(L"Starting checking");

  auto *NIFShader = getNIF()->GetShader(&NIFShape);

  // Get NIFShader type
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_ENVMAP && NIFShaderType != BSLSP_PARALLAX &&
      (NIFShaderType != BSLSP_MULTILAYERPARALLAX || !DisableMLP)) {
    Logger::trace(L"Shape Rejected: Incorrect NIFShader type");
    return false;
  }

  // check to make sure there are textures defined in slot 3 or 8
  if (NIFShaderType != BSLSP_MULTILAYERPARALLAX &&
      (!NIFUtil::getTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::GLOW).empty() ||
       !NIFUtil::getTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::MULTILAYER).empty() ||
       !NIFUtil::getTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::BACKLIGHT).empty())) {
    Logger::trace(L"Shape Rejected: Texture defined in slots 3,7,or 8");
    return false;
  }

  Logger::trace(L"Shape Accepted");
  return true;
}

auto PatcherComplexMaterial::shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool {
  return shouldApply(getTextureSet(NIFShape), Matches);
}

auto PatcherComplexMaterial::shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         std::vector<PatcherMatch> &Matches) -> bool {
  static const auto CMBaseMap = getPGD()->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);

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
    if (BaseMap.empty() || !getPGD()->isFile(BaseMap)) {
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

  PatcherMatch LastMatch; // Variable to store the match that equals OldSlots[Slot], if found
  for (const auto &Match : FoundMatches) {
    if (getPGD3D()->checkIfAspectRatioMatches(BaseMap, Match.Path)) {
      PatcherMatch CurMatch;
      CurMatch.MatchedPath = Match.Path;
      CurMatch.MatchedFrom.insert(MatchedFromSlot);
      if (Match.Path == OldSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)]) {
        LastMatch = CurMatch; // Save the match that equals OldSlots[Slot]
      } else {
        Matches.push_back(CurMatch); // Add other matches
      }
    }
  }

  if (!LastMatch.MatchedPath.empty()) {
    Matches.push_back(LastMatch); // Add the match that equals OldSlots[Slot]
  }

  return !Matches.empty();
}

auto PatcherComplexMaterial::applyPatch(NiShape &NIFShape, const PatcherMatch &Match,
                                        bool &NIFModified, bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  // Prep
  ShapeDeleted = false;

  // Apply shader
  applyShader(NIFShape, NIFModified);

  // Apply slots
  auto NewSlots = applyPatchSlots(getTextureSet(NIFShape), Match);
  setTextureSet(NIFShape, NewSlots, NIFModified);

  return NewSlots;
}

auto PatcherComplexMaterial::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  const auto MatchedPath = Match.MatchedPath;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = L"";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = MatchedPath;

  bool EnableDynCubemaps = !(ParallaxGenDirectory::checkGlobMatchInVector(getNIFPath().wstring(), DynCubemapBlocklist) ||
                             ParallaxGenDirectory::checkGlobMatchInVector(MatchedPath, DynCubemapBlocklist));
  if (EnableDynCubemaps) {
    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)] = L"textures\\cubemaps\\dynamic1pxcubemap_black.dds";
  }

  return NewSlots;
}

void PatcherComplexMaterial::applyShader(NiShape &NIFShape, bool &NIFModified) {
  auto *NIFShader = getNIF()->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Remove texture slots if disabling MLP
  if (DisableMLP && NIFShaderBSLSP->GetShaderType() == BSLSP_MULTILAYERPARALLAX) {
    NIFUtil::setTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::GLOW, "", NIFModified);
    NIFUtil::setTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::MULTILAYER, "", NIFModified);
    NIFUtil::setTextureSlot(getNIF(), &NIFShape, NIFUtil::TextureSlots::BACKLIGHT, "", NIFModified);

    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, NIFModified);
  }

  // Set NIFShader type to env map
  NIFUtil::setShaderType(NIFShader, BSLSP_ENVMAP, NIFModified);
  NIFUtil::setShaderFloat(NIFShaderBSLSP->environmentMapScale, 1.0F, NIFModified);
  // Set NIFShader flags
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
}

auto PatcherComplexMaterial::applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {

  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = Slots;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = L"";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = L"textures\\parallaxgen\\neutral_m.dds";

  return NewSlots;
}
