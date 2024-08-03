#include "NIFUtil.hpp"
#include <NifFile.hpp>
#include <boost/algorithm/string.hpp>

#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;

auto NIFUtil::setShaderType(nifly::NiShader *NIFShader, const nifly::BSLightingShaderPropertyShaderType &Type,
                            bool &Changed) -> void {
  if (NIFShader->GetShaderType() != Type) {
    NIFShader->SetShaderType(Type);
    Changed = true;
  }
}

auto NIFUtil::setShaderFloat(float &Value, const float &NewValue, bool &Changed) -> void {
  if (Value != NewValue) {
    Value = NewValue;
    Changed = true;
  }
}

auto NIFUtil::setShaderVec2(nifly::Vector2 &Value, const nifly::Vector2 &NewValue, bool &Changed) -> void {
  if (Value != NewValue) {
    Value = NewValue;
    Changed = true;
  }
}

// Shader flag helpers
auto NIFUtil::hasShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags1 &Flag) -> bool {
  return (NIFShaderBSLSP->shaderFlags1 & Flag) != 0U;
}

auto NIFUtil::hasShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags2 &Flag) -> bool {
  return (NIFShaderBSLSP->shaderFlags2 & Flag) != 0U;
}

auto NIFUtil::setShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags1 &Flag, bool &Changed) -> void {
  if (!hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags1 |= Flag;
    Changed = true;
  }
}

auto NIFUtil::setShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags2 &Flag, bool &Changed) -> void {
  if (!hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags2 |= Flag;
    Changed = true;
  }
}

auto NIFUtil::clearShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                              const nifly::SkyrimShaderPropertyFlags1 &Flag, bool &Changed) -> void {
  if (hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags1 &= ~Flag;
    Changed = true;
  }
}

auto NIFUtil::clearShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                              const nifly::SkyrimShaderPropertyFlags2 &Flag, bool &Changed) -> void {
  if (hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags2 &= ~Flag;
    Changed = true;
  }
}

auto NIFUtil::configureShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                                  const nifly::SkyrimShaderPropertyFlags1 &Flag, const bool &Enable,
                                  bool &Changed) -> void {
  if (Enable) {
    setShaderFlag(NIFShaderBSLSP, Flag, Changed);
  } else {
    clearShaderFlag(NIFShaderBSLSP, Flag, Changed);
  }
}

auto NIFUtil::configureShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                                  const nifly::SkyrimShaderPropertyFlags2 &Flag, const bool &Enable,
                                  bool &Changed) -> void {
  if (Enable) {
    setShaderFlag(NIFShaderBSLSP, Flag, Changed);
  } else {
    clearShaderFlag(NIFShaderBSLSP, Flag, Changed);
  }
}

// Texture slot helpers
auto NIFUtil::setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot,
                             const string &TexturePath, bool &Changed) -> void {
  string ExistingTex;
  NIF->GetTextureSlot(NIFShape, ExistingTex, static_cast<unsigned int>(Slot));
  if (!boost::iequals(ExistingTex, TexturePath)) {
    auto NewTex = TexturePath;
    NIF->SetTextureSlot(NIFShape, NewTex, static_cast<unsigned int>(Slot));
    Changed = true;
  }
}

auto NIFUtil::getTexBase(const wstring &TexPath, const NIFUtil::TextureSlots &Slot,
                         ParallaxGenConfig *PGC) -> std::wstring {
  auto SuffixList = ParallaxGenUtil::stringVecToWstringVec(
      PGC->getConfig()["suffixes"][to_string(static_cast<unsigned int>(Slot))].get<vector<string>>());

  // Loop through SuffixList
  for (const auto &Suffix : SuffixList) {
    // Check if the texture path ends with the suffix
    if (boost::iends_with(TexPath, Suffix)) {
      // Remove the suffix from the texture path
      return TexPath.substr(0, TexPath.size() - Suffix.size());
    }
  }

  return {};
}

auto NIFUtil::getTexMatch(const wstring &Base, const NIFUtil::TextureSlots &Slot, ParallaxGenConfig *PGC,
                          ParallaxGenDirectory *PGD) -> std::filesystem::path {
  vector<filesystem::path> SearchList;
  switch (Slot) {
  case TextureSlots::Parallax:
    SearchList = PGD->getHeightMaps();
    break;
  case TextureSlots::EnvMask:
    SearchList = PGD->getComplexMaterialMaps();
    break;
  default:
    return {};
  }

  for (const auto &Search : SearchList) {
    auto SearchBase = getTexBase(Search.wstring(), Slot, PGC);
    if (boost::iequals(SearchBase, Base)) {
      return Search;
    }
  }

  return {};
}

auto NIFUtil::getSearchPrefixes(NifFile &NIF, nifly::NiShape *NIFShape,
                                ParallaxGenConfig *PGC) -> array<string, NUM_TEXTURE_SLOTS> {
  array<string, NUM_TEXTURE_SLOTS> OutPrefixes;

  // Loop through each texture Slot
  for (uint32_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
    string Texture;
    uint32_t Result = NIF.GetTextureSlot(NIFShape, Texture, I);

    if (Result == 0 || Texture.empty()) {
      // no texture in Slot
      continue;
    }

    // Get default suffixes
    const auto TexBase = getTexBase(ParallaxGenUtil::stringToWstring(Texture), static_cast<TextureSlots>(I), PGC);
    OutPrefixes[I] = ParallaxGenUtil::wstringToString(TexBase);
  }

  return OutPrefixes;
}
