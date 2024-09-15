#include "NIFUtil.hpp"
#include <NifFile.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/iostreams/stream.hpp>

#include "ParallaxGenUtil.hpp"

using namespace std;

// TODO this is a headache we should just use ints
auto NIFUtil::getDefaultTextureType(const TextureSlots &Slot) -> PGTextureType {
  switch (Slot) {
    case TextureSlots::Diffuse:
      return PGTextureType::DIFFUSE;
    case TextureSlots::Normal:
      return PGTextureType::NORMAL;
    case TextureSlots::Glow:
      return PGTextureType::EMISSIVE;
    case TextureSlots::Parallax:
      return PGTextureType::HEIGHT;
    case TextureSlots::Cubemap:
      return PGTextureType::CUBEMAP;
    case TextureSlots::EnvMask:
      return PGTextureType::ENVIRONMENTMASK;
    case TextureSlots::Tint:
      return PGTextureType::TINT;
    case TextureSlots::Backlight:
      return PGTextureType::BACKLIGHT;
    default:
      return PGTextureType::DIFFUSE;
  }
}

auto NIFUtil::getTexSuffixMap() -> map<wstring, tuple<NIFUtil::TextureSlots, NIFUtil::PGTextureType>> {
  static const map<wstring, tuple<NIFUtil::TextureSlots, NIFUtil::PGTextureType>> TextureSuffixMap = {
    {L"_bl", {NIFUtil::TextureSlots::Backlight, NIFUtil::PGTextureType::BACKLIGHT}},
    {L"_b", {NIFUtil::TextureSlots::Backlight, NIFUtil::PGTextureType::BACKLIGHT}},
    {L"_cnr", {NIFUtil::TextureSlots::Tint, NIFUtil::PGTextureType::COATNORMAL}},
    {L"_s", {NIFUtil::TextureSlots::Tint, NIFUtil::PGTextureType::SUBSURFACE}},  // TODO verify this
    {L"_i", {NIFUtil::TextureSlots::Tint, NIFUtil::PGTextureType::INNERLAYER}},
    {L"_rmaos", {NIFUtil::TextureSlots::EnvMask, NIFUtil::PGTextureType::RMAOS}},
    {L"_envmask", {NIFUtil::TextureSlots::EnvMask, NIFUtil::PGTextureType::ENVIRONMENTMASK}},
    {L"_em", {NIFUtil::TextureSlots::EnvMask, NIFUtil::PGTextureType::ENVIRONMENTMASK}},
    {L"_m", {NIFUtil::TextureSlots::EnvMask, NIFUtil::PGTextureType::ENVIRONMENTMASK}},
    {L"_e", {NIFUtil::TextureSlots::Cubemap, NIFUtil::PGTextureType::CUBEMAP}},
    {L"_p", {NIFUtil::TextureSlots::Parallax, NIFUtil::PGTextureType::HEIGHT}},
    {L"_sk", {NIFUtil::TextureSlots::Glow, NIFUtil::PGTextureType::EMISSIVE}},  // TODO this aint right
    {L"_g", {NIFUtil::TextureSlots::Glow, NIFUtil::PGTextureType::EMISSIVE}},
    {L"_msn", {NIFUtil::TextureSlots::Normal, NIFUtil::PGTextureType::NORMAL}},
    {L"_n", {NIFUtil::TextureSlots::Normal, NIFUtil::PGTextureType::NORMAL}},
    {L"_d", {NIFUtil::TextureSlots::Diffuse, NIFUtil::PGTextureType::DIFFUSE}},
    {L"mask", {NIFUtil::TextureSlots::Diffuse, NIFUtil::PGTextureType::DIFFUSE}}
  };

  return TextureSuffixMap;
}

auto NIFUtil::getTextureTypeStr(const PGTextureType &Type) -> string {
  switch (Type) {
    case PGTextureType::DIFFUSE:
      return "Diffuse";
    case PGTextureType::NORMAL:
      return "Normal";
    case PGTextureType::MODELSPACENORMAL:
      return "Model Space Normal";
    case PGTextureType::EMISSIVE:
      return "Emissive";
    case PGTextureType::SKINTINT:
      return "Skin Tint";
    case PGTextureType::SUBSURFACE:
      return "Subsurface";
    case PGTextureType::HEIGHT:
      return "Height";
    case PGTextureType::CUBEMAP:
      return "Cubemap";
    case PGTextureType::ENVIRONMENTMASK:
      return "Environment Mask";
    case PGTextureType::COMPLEXMATERIAL:
      return "Complex Material";
    case PGTextureType::RMAOS:
      return "RMAOS";
    case PGTextureType::TINT:
      return "Tint";
    case PGTextureType::INNERLAYER:
      return "Inner Layer";
    case PGTextureType::COATNORMAL:
      return "Coat Normal";
    case PGTextureType::BACKLIGHT:
      return "Backlight";
    case PGTextureType::SPECULAR:
      return "Specular";
    default:
      return "Unknown";
  }
}

auto NIFUtil::getDefaultsFromSuffix(const std::filesystem::path &Path) -> tuple<NIFUtil::TextureSlots, NIFUtil::PGTextureType> {
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
  return SuffixMap.at(L"_d");
}

auto NIFUtil::loadNIFFromBytes(const std::vector<std::byte> &NIFBytes) -> nifly::NifFile {
  // NIF file object
  NifFile NIF;

  // Get NIF Bytes
  if (NIFBytes.empty()) {
    throw runtime_error("Unable to load NIF: File is empty");
  }

  // Convert Byte Vector to Stream
  boost::iostreams::array_source NIFArraySource(reinterpret_cast<const char *>(NIFBytes.data()), NIFBytes.size());
  boost::iostreams::stream<boost::iostreams::array_source> NIFStream(NIFArraySource);

  NIF.Load(NIFStream);
  if (!NIF.IsValid()) {
    throw runtime_error("Unable to load NIF: Invalid NIF");
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
auto NIFUtil::hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags1 &Flag) -> bool {
  return (NIFShaderBSLSP->shaderFlags1 & Flag) != 0U;
}

auto NIFUtil::hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags2 &Flag) -> bool {
  return (NIFShaderBSLSP->shaderFlags2 & Flag) != 0U;
}

auto NIFUtil::setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags1 &Flag, bool &Changed) -> void {
  if (!hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags1 |= Flag;
    Changed = true;
  }
}

auto NIFUtil::setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                            const nifly::SkyrimShaderPropertyFlags2 &Flag, bool &Changed) -> void {
  if (!hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags2 |= Flag;
    Changed = true;
  }
}

auto NIFUtil::clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                              const nifly::SkyrimShaderPropertyFlags1 &Flag, bool &Changed) -> void {
  if (hasShaderFlag(NIFShaderBSLSP, Flag)) {
    NIFShaderBSLSP->shaderFlags1 &= ~Flag;
    Changed = true;
  }
}

auto NIFUtil::clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                              const nifly::SkyrimShaderPropertyFlags2 &Flag, bool &Changed) -> void {
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
auto NIFUtil::setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot, const std::wstring &TexturePath, bool &Changed) -> void {
  auto TexturePathStr = ParallaxGenUtil::wstringToString(TexturePath);
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

auto NIFUtil::getTexMatch(const wstring &Base, const map<wstring, PGTexture> &SearchMap) -> PGTexture {
  // Binary search on base list
  const wstring BaseLower = boost::to_lower_copy(Base);
  const auto It = SearchMap.find(BaseLower);

  if (It != SearchMap.end()) {
    // Found a match
    if (!boost::equals(It->first, BaseLower)) {
      // No exact match
      return {};
    }

    // Return matched texture
    return It->second;
  }

  return {};
}

auto NIFUtil::getSearchPrefixes(NifFile &NIF, nifly::NiShape *NIFShape) -> array<wstring, NUM_TEXTURE_SLOTS> {
  array<wstring, NUM_TEXTURE_SLOTS> OutPrefixes;

  // Loop through each texture Slot
  for (uint32_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
    string Texture;
    uint32_t Result = NIF.GetTextureSlot(NIFShape, Texture, I);

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
