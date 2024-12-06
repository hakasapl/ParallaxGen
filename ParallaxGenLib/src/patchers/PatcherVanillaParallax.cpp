#include "patchers/PatcherVanillaParallax.hpp"

#include <Geometry.hpp>
#include <boost/algorithm/string.hpp>

#include "Logger.hpp"
#include "NIFUtil.hpp"

using namespace std;

auto PatcherVanillaParallax::getFactory() -> PatcherShader::PatcherShaderFactory {
  return [](const filesystem::path& NIFPath, nifly::NifFile *NIF) -> unique_ptr<PatcherShader> {
    return make_unique<PatcherVanillaParallax>(NIFPath, NIF);
  };
}

auto PatcherVanillaParallax::getShaderType() -> NIFUtil::ShapeShader {
  return NIFUtil::ShapeShader::VANILLAPARALLAX;
}

PatcherVanillaParallax::PatcherVanillaParallax(filesystem::path NIFPath, nifly::NifFile *NIF)
    : PatcherShader(std::move(NIFPath), NIF, "VanillaParallax") {
  // Determine if NIF has attached havok animations
  vector<NiObject *> NIFBlockTree;
  NIF->GetTree(NIFBlockTree);

  for (NiObject *NIFBlock : NIFBlockTree) {
    if (boost::iequals(NIFBlock->GetBlockName(), "BSBehaviorGraphExtraData")) {
      HasAttachedHavok = true;
    }
  }
}

auto PatcherVanillaParallax::canApply(NiShape &NIFShape) -> bool {
  auto *NIFShader = getNIF()->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  // Check if nif has attached havok (Results in crashes for vanilla Parallax)
  if (HasAttachedHavok) {
    Logger::trace(L"Cannot Apply: Attached havok animations");
    return false;
  }

  // ignore skinned meshes, these don't support Parallax
  if (NIFShape.HasSkinInstance() || NIFShape.IsSkinned()) {
    Logger::trace(L"Cannot Apply: Skinned mesh");
    return false;
  }

  // Check for shader type
  auto NIFShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(NIFShader->GetShaderType());
  if (NIFShaderType != BSLSP_DEFAULT && NIFShaderType != BSLSP_PARALLAX && NIFShaderType != BSLSP_ENVMAP) {
    // don't overwrite existing NIFShaders
    Logger::trace(L"Cannot Apply: Incorrect NIFShader type");
    return false;
  }

  // decals don't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DECAL) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_DYNAMIC_DECAL)) {
    Logger::trace(L"Cannot Apply: Shape has decal");
    return false;
  }

  // Mesh lighting doesn't work with regular Parallax
  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_SOFT_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_RIM_LIGHTING) ||
      NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING)) {
    Logger::trace(L"Cannot Apply: Lighting on shape");
    return false;
  }

  return true;
}

auto PatcherVanillaParallax::shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool {
  return shouldApply(getTextureSet(NIFShape), Matches);
}

auto PatcherVanillaParallax::shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         std::vector<PatcherMatch> &Matches) -> bool {
  static const auto HeightBaseMap = getPGD()->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);

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
    if (BaseMap.empty() || !getPGD()->isFile(BaseMap)) {
      continue;
    }

    FoundMatches.clear();
    FoundMatches = NIFUtil::getTexMatch(SearchPrefixes[Slot], NIFUtil::TextureType::HEIGHT, HeightBaseMap);

    if (!FoundMatches.empty()) {
      // TODO should we be trying diffuse after normal too and present all options?
      MatchedFromSlot = static_cast<NIFUtil::TextureSlots>(Slot);
      break;
    }
  }

  // Check aspect ratio matches
  PatcherMatch LastMatch; // Variable to store the match that equals OldSlots[Slot], if found
  for (const auto &Match : FoundMatches) {
    if (getPGD3D()->checkIfAspectRatioMatches(BaseMap, Match.Path)) {
      PatcherMatch CurMatch;
      CurMatch.MatchedPath = Match.Path;
      CurMatch.MatchedFrom.insert(MatchedFromSlot);
      if (Match.Path == OldSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)]) {
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

auto PatcherVanillaParallax::applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match,
                                        bool &NIFModified, bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  // Prep
  ShapeDeleted = false;
  auto *NIFShader = getNIF()->GetShader(&NIFShape);
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

  auto NewSlots = applyPatchSlots(getTextureSet(NIFShape), Match);
  setTextureSet(NIFShape, NewSlots, NIFModified);

  return NewSlots;
}

auto PatcherVanillaParallax::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = Match.MatchedPath;

  return NewSlots;
}

auto PatcherVanillaParallax::applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {

  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = Slots;

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = L"textures\\parallaxgen\\neutral_p.dds";

  return NewSlots;
}
