#include "NIFUtil.hpp"
#include <NifFile.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/iostreams/stream.hpp>

#include "ParallaxGenUtil.hpp"

using namespace std;

auto NIFUtil::getDefaultTextureType(const TextureSlots &Slot) -> PGTextureType {
  switch (Slot) {
    case TextureSlots::Diffuse:
      return PGTextureType::DIFFUSE;
    case TextureSlots::Normal:
      return PGTextureType::NORMAL;
    case TextureSlots::Glow:
      return PGTextureType::GLOW;
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

auto NIFUtil::getTexSuffixMap() -> map<wstring, NIFUtil::TextureSlots, ReverseComparator> {
  static const map<wstring, NIFUtil::TextureSlots, ReverseComparator> TextureSuffixMap = {
    {L"_bl.dds", NIFUtil::TextureSlots::Backlight},
    {L"_b.dds", NIFUtil::TextureSlots::Backlight},
    {L"_cnr.dds", NIFUtil::TextureSlots::Tint},
    {L"_s.dds", NIFUtil::TextureSlots::Tint},
    {L"_rmaos.dds", NIFUtil::TextureSlots::EnvMask},
    {L"_envmask.dds", NIFUtil::TextureSlots::EnvMask},
    {L"_em.dds", NIFUtil::TextureSlots::EnvMask},
    {L"_m.dds", NIFUtil::TextureSlots::EnvMask},
    {L"_e.dds", NIFUtil::TextureSlots::Cubemap},
    {L"_p.dds", NIFUtil::TextureSlots::Parallax},
    {L"_sk.dds", NIFUtil::TextureSlots::Glow},
    {L"_g.dds", NIFUtil::TextureSlots::Glow},
    {L"_msn.dds", NIFUtil::TextureSlots::Normal},
    {L"_n.dds", NIFUtil::TextureSlots::Normal},
    {L"_d.dds", NIFUtil::TextureSlots::Diffuse},
    {L"mask.dds", NIFUtil::TextureSlots::Diffuse},
    {L".dds", NIFUtil::TextureSlots::Diffuse}
  };

  return TextureSuffixMap;
}

auto NIFUtil::getSlotFromPath(const std::filesystem::path &Path) -> NIFUtil::TextureSlots {
  const auto &SuffixMap = getTexSuffixMap();

  // Get the texture suffix
  const auto &PathStr = Path.wstring();
  auto It = SuffixMap.lower_bound(PathStr);

  if (It != SuffixMap.end() && !boost::ends_with(PathStr, It->first)) {
    return TextureSlots::Diffuse;  // Default to diffuse
  }

  return It->second;
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

auto NIFUtil::getTexBase(const std::filesystem::path &TexPath) -> std::wstring {
  return getTexBase(TexPath.wstring());
}

auto NIFUtil::getTexBase(const wstring &TexPath) -> std::wstring {
  const auto &SuffixMap = getTexSuffixMap();

  // Get the texture suffix
  auto It = SuffixMap.lower_bound(TexPath);

  if (It != SuffixMap.end() && !boost::ends_with(TexPath, It->first)) {
    return TexPath;
  }

  return TexPath.substr(0, TexPath.size() - It->first.size());
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
