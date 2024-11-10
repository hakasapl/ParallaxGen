#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"

#include <nifly/Geometry.hpp>
#include <nifly/NifFile.hpp>
#include <nifly/Object3d.hpp>
#include <nifly/Shaders.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

#include <array>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>


#include <cstddef>
#include <cstdint>

using namespace std;

auto NIFUtil::getStrFromShader(const ShapeShader &Shader) -> string {
  const static unordered_map<NIFUtil::ShapeShader, string> StrFromShaderMap = {
      {ShapeShader::NONE, "None"},
      {ShapeShader::TRUEPBR, "PBR"},
      {ShapeShader::COMPLEXMATERIAL, "Complex Material"},
      {ShapeShader::VANILLAPARALLAX, "Parallax"}};

  if (StrFromShaderMap.find(Shader) != StrFromShaderMap.end()) {
    return StrFromShaderMap.at(Shader);
  }

  return StrFromShaderMap.at(ShapeShader::NONE);
}

auto NIFUtil::getDefaultTextureType(const TextureSlots &Slot) -> TextureType {
  switch (Slot) {
  case TextureSlots::DIFFUSE:
    return TextureType::DIFFUSE;
  case TextureSlots::NORMAL:
    return TextureType::NORMAL;
  case TextureSlots::GLOW:
    return TextureType::EMISSIVE;
  case TextureSlots::PARALLAX:
    return TextureType::HEIGHT;
  case TextureSlots::CUBEMAP:
    return TextureType::CUBEMAP;
  case TextureSlots::ENVMASK:
    return TextureType::ENVIRONMENTMASK;
  case TextureSlots::MULTILAYER:
    return TextureType::SUBSURFACETINT;
  case TextureSlots::BACKLIGHT:
    return TextureType::BACKLIGHT;
  default:
    return TextureType::DIFFUSE;
  }
}

auto NIFUtil::getTexSuffixMap() -> map<wstring, tuple<NIFUtil::TextureSlots, NIFUtil::TextureType>> {
  static const map<wstring, tuple<NIFUtil::TextureSlots, NIFUtil::TextureType>> TextureSuffixMap = {
      {L"_bl", {NIFUtil::TextureSlots::BACKLIGHT, NIFUtil::TextureType::BACKLIGHT}},
      {L"_b", {NIFUtil::TextureSlots::BACKLIGHT, NIFUtil::TextureType::BACKLIGHT}},
      {L"_cnr", {NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::COATNORMALROUGHNESS}},
      {L"_s", {NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::SUBSURFACETINT}},
      {L"_i", {NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::INNERLAYER}},
      {L"_rmaos", {NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::RMAOS}},
      {L"_envmask", {NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::ENVIRONMENTMASK}},
      {L"_em", {NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::ENVIRONMENTMASK}},
      {L"_m", {NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::ENVIRONMENTMASK}},
      {L"_e", {NIFUtil::TextureSlots::CUBEMAP, NIFUtil::TextureType::CUBEMAP}},
      {L"_p", {NIFUtil::TextureSlots::PARALLAX, NIFUtil::TextureType::HEIGHT}},
      {L"_sk", {NIFUtil::TextureSlots::GLOW, NIFUtil::TextureType::SKINTINT}},
      {L"_g", {NIFUtil::TextureSlots::GLOW, NIFUtil::TextureType::EMISSIVE}},
      {L"_msn", {NIFUtil::TextureSlots::NORMAL, NIFUtil::TextureType::NORMAL}},
      {L"_n", {NIFUtil::TextureSlots::NORMAL, NIFUtil::TextureType::NORMAL}},
      {L"_d", {NIFUtil::TextureSlots::DIFFUSE, NIFUtil::TextureType::DIFFUSE}},
      {L"mask", {NIFUtil::TextureSlots::DIFFUSE, NIFUtil::TextureType::DIFFUSE}}};

  return TextureSuffixMap;
}

auto NIFUtil::getStrFromTexType(const TextureType &Type) -> string {
  static unordered_map<TextureType, string> StrFromTexMap = {{TextureType::DIFFUSE, "diffuse"},
                                                             {TextureType::NORMAL, "normal"},
                                                             {TextureType::MODELSPACENORMAL, "model space normal"},
                                                             {TextureType::EMISSIVE, "emissive"},
                                                             {TextureType::SKINTINT, "skin tint"},
                                                             {TextureType::SUBSURFACECOLOR, "subsurface color"},
                                                             {TextureType::HEIGHT, "height"},
                                                             {TextureType::CUBEMAP, "cubemap"},
                                                             {TextureType::ENVIRONMENTMASK, "environment mask"},
                                                             {TextureType::COMPLEXMATERIAL, "complex material"},
                                                             {TextureType::RMAOS, "rmaos"},
                                                             {TextureType::SUBSURFACETINT, "subsurface tint"},
                                                             {TextureType::INNERLAYER, "inner layer"},
                                                             {TextureType::COATNORMALROUGHNESS, "coat normal roughness"},
                                                             {TextureType::BACKLIGHT, "backlight"},
                                                             {TextureType::SPECULAR, "specular"},
                                                             {TextureType::SUBSURFACEPBR, "subsurface pbr"},
                                                             {TextureType::UNKNOWN, "unknown"}};

  if (StrFromTexMap.find(Type) != StrFromTexMap.end()) {
    return StrFromTexMap[Type];
  }

  return StrFromTexMap[TextureType::UNKNOWN];
}

auto NIFUtil::getTexTypeFromStr(const string &Type) -> TextureType {
  static unordered_map<string, TextureType> TexFromStrMap = {{"diffuse", TextureType::DIFFUSE},
                                                             {"normal", TextureType::NORMAL},
                                                             {"model space normal", TextureType::MODELSPACENORMAL},
                                                             {"emissive", TextureType::EMISSIVE},
                                                             {"skin tint", TextureType::SKINTINT},
                                                             {"subsurface color", TextureType::SUBSURFACECOLOR},
                                                             {"height", TextureType::HEIGHT},
                                                             {"cubemap", TextureType::CUBEMAP},
                                                             {"environment mask", TextureType::ENVIRONMENTMASK},
                                                             {"complex material", TextureType::COMPLEXMATERIAL},
                                                             {"rmaos", TextureType::RMAOS},
                                                             {"subsurface tint", TextureType::SUBSURFACETINT},
                                                             {"inner layer", TextureType::INNERLAYER},
                                                             {"coat normal roughness", TextureType::COATNORMALROUGHNESS},
                                                             {"backlight", TextureType::BACKLIGHT},
                                                             {"specular", TextureType::SPECULAR},
                                                             {"subsurface pbr", TextureType::SUBSURFACEPBR},
                                                             {"unknown", TextureType::UNKNOWN}};

  const auto SearchKey = boost::to_lower_copy(Type);
  if (TexFromStrMap.find(Type) != TexFromStrMap.end()) {
    return TexFromStrMap[Type];
  }

  return TexFromStrMap["unknown"];
}

auto NIFUtil::getSlotFromTexType(const TextureType &Type) -> TextureSlots {
  static unordered_map<TextureType, TextureSlots> TexTypeToSlotMap = {
      {TextureType::DIFFUSE, TextureSlots::DIFFUSE},         {TextureType::NORMAL, TextureSlots::NORMAL},
      {TextureType::MODELSPACENORMAL, TextureSlots::NORMAL}, {TextureType::EMISSIVE, TextureSlots::GLOW},
      {TextureType::SKINTINT, TextureSlots::GLOW},           {TextureType::SUBSURFACECOLOR, TextureSlots::GLOW},
      {TextureType::HEIGHT, TextureSlots::PARALLAX},         {TextureType::CUBEMAP, TextureSlots::CUBEMAP},
      {TextureType::ENVIRONMENTMASK, TextureSlots::ENVMASK}, {TextureType::COMPLEXMATERIAL, TextureSlots::ENVMASK},
      {TextureType::RMAOS, TextureSlots::ENVMASK},           {TextureType::SUBSURFACETINT, TextureSlots::MULTILAYER},
      {TextureType::INNERLAYER, TextureSlots::MULTILAYER},   {TextureType::COATNORMALROUGHNESS, TextureSlots::MULTILAYER},
      {TextureType::BACKLIGHT, TextureSlots::BACKLIGHT},     {TextureType::SPECULAR, TextureSlots::BACKLIGHT},
      {TextureType::SUBSURFACEPBR, TextureSlots::BACKLIGHT}, {TextureType::UNKNOWN, TextureSlots::UNKNOWN}};

  if (TexTypeToSlotMap.find(Type) != TexTypeToSlotMap.end()) {
    return TexTypeToSlotMap[Type];
  }

  return TexTypeToSlotMap[TextureType::UNKNOWN];
}

auto NIFUtil::getDefaultsFromSuffix(const std::filesystem::path &Path)
    -> tuple<NIFUtil::TextureSlots, NIFUtil::TextureType> {
  const auto &SuffixMap = getTexSuffixMap();

  // Get the texture suffix
  const auto PathWithoutExtension = Path.parent_path() / Path.stem();
  const auto &PathStr = PathWithoutExtension.wstring();

  for (const auto &[Suffix, Slot] : SuffixMap) {
    if (boost::iends_with(PathStr, Suffix)) {
      return Slot;
    }
  }

  // Default return diffuse
  return {TextureSlots::UNKNOWN, TextureType::UNKNOWN};
}

auto NIFUtil::loadNIFFromBytes(const std::vector<std::byte> &NIFBytes) -> nifly::NifFile {
  // NIF file object
  NifFile NIF;

  // Get NIF Bytes
  if (NIFBytes.empty()) {
    throw runtime_error("File is empty");
  }

  // Convert Byte Vector to Stream
  boost::iostreams::array_source NIFArraySource(reinterpret_cast<const char *>(NIFBytes.data()), NIFBytes.size());
  boost::iostreams::stream<boost::iostreams::array_source> NIFStream(NIFArraySource);

  NIF.Load(NIFStream);
  if (!NIF.IsValid()) {
    throw runtime_error("Invalid NIF");
  }

  return NIF;
}

auto NIFUtil::setShaderType(nifly::NiShader *NIFShader, const nifly::BSLightingShaderPropertyShaderType &Type,
                            bool &Changed) -> void {
  if (NIFShader->GetShaderType() != Type) {
    NIFShader->SetShaderType(Type);
    Changed = true;
  }
}

auto NIFUtil::setShaderFloat(float &Value, const float &NewValue, bool &Changed) -> void {
  if (fabs(Value - NewValue) > 10e-05f) {
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
auto NIFUtil::hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags1 &Flag) -> bool {
  return (NIFShaderBSLSP->shaderFlags1 & Flag) != 0U;
}

auto NIFUtil::hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags2 &Flag) -> bool {
  return (NIFShaderBSLSP->shaderFlags2 & Flag) != 0U;
}

auto NIFUtil::setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                            bool &Changed) -> void {
  if (!hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags1 |= Flag;
    Changed = true;
  }
}

auto NIFUtil::setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                            bool &Changed) -> void {
  if (!hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags2 |= Flag;
    Changed = true;
  }
}

auto NIFUtil::clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                              bool &Changed) -> void {
  if (hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags1 &= ~Flag;
    Changed = true;
  }
}

auto NIFUtil::clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                              bool &Changed) -> void {
  if (hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags2 &= ~Flag;
    Changed = true;
  }
}

auto NIFUtil::configureShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                                  const nifly::SkyrimShaderPropertyFlags1 &Flag, const bool &Enable,
                                  bool &Changed) -> void {
  if (Enable) {
    setShaderFlag(NIFShaderBSLSP, Flag, Changed);
  } else {
    clearShaderFlag(NIFShaderBSLSP, Flag, Changed);
  }
}

auto NIFUtil::configureShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
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
                             const std::wstring &TexturePath, bool &Changed) -> void {
  auto TexturePathStr = ParallaxGenUtil::wstrToStr(TexturePath);
  setTextureSlot(NIF, NIFShape, Slot, TexturePathStr, Changed);
}

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

auto NIFUtil::setTextureSlots(nifly::NifFile *NIF, nifly::NiShape *NIFShape,
                              const std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots, bool &Changed) -> void {
  for (uint32_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
    setTextureSlot(NIF, NIFShape, static_cast<TextureSlots>(I), NewSlots[I], Changed);
  }
}

auto NIFUtil::getTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot) -> string {
  string Texture;
  NIF->GetTextureSlot(NIFShape, Texture, static_cast<unsigned int>(Slot));
  return Texture;
}

auto NIFUtil::getTextureSlots(nifly::NifFile *NIF, nifly::NiShape *NIFShape) -> array<wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> OutSlots;

  for (uint32_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
    string Texture;
    const uint32_t Result = NIF->GetTextureSlot(NIFShape, Texture, I);

    if (Result == 0 || Texture.empty()) {
      // no texture in Slot
      continue;
    }

    OutSlots[I] = ParallaxGenUtil::strToWstr(Texture);
  }

  return OutSlots;
}

auto NIFUtil::getTexBase(const std::filesystem::path &Path) -> std::wstring {
  const auto &SuffixMap = getTexSuffixMap();

  // Get the texture suffix
  const auto PathWithoutExtension = Path.parent_path() / Path.stem();
  const auto &PathStr = PathWithoutExtension.wstring();

  for (const auto &[Suffix, Slot] : SuffixMap) {
    if (boost::iends_with(PathStr, Suffix)) {
      return PathStr.substr(0, PathStr.size() - Suffix.size());
    }
  }

  return PathStr;
}

auto NIFUtil::getTexMatch(const wstring &Base, const TextureType &DesiredType,
                          const map<wstring, unordered_set<PGTexture, PGTextureHasher>> &SearchMap) -> vector<PGTexture> {
  // Binary search on base list
  const wstring BaseLower = boost::to_lower_copy(Base);
  const auto It = SearchMap.find(BaseLower);

  if (It != SearchMap.end()) {
    // Found a match
    if (!boost::equals(It->first, BaseLower)) {
      // No exact match
      return {};
    }

    if (It->second.empty()) {
      // No textures
      return {};
    }

    vector<PGTexture> OutTex;
    for (const auto &Texture : It->second) {
      if (Texture.Type == DesiredType) {
        OutTex.push_back(Texture);
      } else {
        continue;
      }
    }

    return OutTex;
  }

  return {};
}

auto NIFUtil::getSearchPrefixes(NifFile const& NIF, nifly::NiShape *NIFShape) -> array<wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> OutPrefixes;

  // Loop through each texture Slot
  for (uint32_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
    string Texture;
    const uint32_t Result = NIF.GetTextureSlot(NIFShape, Texture, I);

    if (Result == 0 || Texture.empty()) {
      // no texture in Slot
      continue;
    }

    // Get default suffixes
    const auto TexBase = getTexBase(Texture);
    OutPrefixes[I] = TexBase;
  }

  return OutPrefixes;
}

auto NIFUtil::getSearchPrefixes(const array<wstring, NUM_TEXTURE_SLOTS> &OldSlots)
    -> array<wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> OutSlots;

  for (uint32_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
    if (OldSlots[I].empty()) {
      continue;
    }

    const auto TexBase = getTexBase(OldSlots[I]);
    OutSlots[I] = TexBase;
  }

  return OutSlots;
}
