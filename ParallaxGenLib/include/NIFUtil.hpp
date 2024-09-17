#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>
#include <Shaders.hpp>
#include <array>
#include <tuple>

#define NUM_TEXTURE_SLOTS 9

namespace NIFUtil {
enum class TextureSlots : unsigned int {
  DIFFUSE,
  NORMAL,
  GLOW,
  PARALLAX,
  CUBEMAP,
  ENVMASK,
  TINT,
  BACKLIGHT,
  UNUSED
};

enum class TextureType {
  DIFFUSE,
  NORMAL,
  MODELSPACENORMAL,
  EMISSIVE,
  SKINTINT,
  SUBSURFACE,
  HEIGHT,
  CUBEMAP,
  ENVIRONMENTMASK,
  COMPLEXMATERIAL,
  RMAOS,
  TINT,
  INNERLAYER,
  COATNORMAL,
  BACKLIGHT,
  SPECULAR
};

auto getTextureTypeStr(const TextureType &Type) -> std::string;

struct PGTexture {
  std::filesystem::path Path;
  TextureType Type;
};

auto getDefaultTextureType(const TextureSlots &Slot) -> TextureType;

auto loadNIFFromBytes(const std::vector<std::byte> &NIFBytes) -> nifly::NifFile;

auto getTexSuffixMap() -> std::map<std::wstring, std::tuple<TextureSlots, TextureType>>;

auto getDefaultsFromSuffix(const std::filesystem::path &Path) -> std::tuple<TextureSlots, TextureType>;

// shader helpers
auto setShaderType(nifly::NiShader *NIFShader, const nifly::BSLightingShaderPropertyShaderType &Type,
                   bool &Changed) -> void;
auto setShaderFloat(float &Value, const float &NewValue, bool &Changed) -> void;
auto setShaderVec2(nifly::Vector2 &Value, const nifly::Vector2 &NewValue, bool &Changed) -> void;

// Shader flag helpers
auto hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                   const nifly::SkyrimShaderPropertyFlags1 &Flag) -> bool;
auto hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP,
                   const nifly::SkyrimShaderPropertyFlags2 &Flag) -> bool;
auto setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                   bool &Changed) -> void;
auto setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                   bool &Changed) -> void;
auto clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                     bool &Changed) -> void;
auto clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                     bool &Changed) -> void;
auto configureShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                         const bool &Enable, bool &Changed) -> void;
auto configureShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                         const bool &Enable, bool &Changed) -> void;

// Texture slot helpers
auto setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot, const std::wstring &TexturePath, bool &Changed) -> void;
auto setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot,
                    const std::string &TexturePath, bool &Changed) -> void;
auto getTexBase(const std::filesystem::path &TexPath) -> std::wstring;
auto getTexMatch(const std::wstring &Base,
                 const std::map<std::wstring, PGTexture> &SearchMap) -> PGTexture;
// Gets all the texture prefixes for a textureset. ie. _n.dds is removed etc. for each slot
auto getSearchPrefixes(nifly::NifFile &NIF, nifly::NiShape *NIFShape)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

} // namespace NIFUtil
