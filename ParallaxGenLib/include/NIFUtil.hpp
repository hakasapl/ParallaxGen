#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>
#include <Shaders.hpp>
#include <array>
#include <tuple>

constexpr unsigned NUM_TEXTURE_SLOTS = 9;

namespace NIFUtil {
enum class ShapeShader {
  NONE,
  TRUEPBR,
  COMPLEXMATERIAL,
  VANILLAPARALLAX
};

enum class TextureSlots : unsigned int {
  DIFFUSE,
  NORMAL,
  GLOW,
  PARALLAX,
  CUBEMAP,
  ENVMASK,
  TINT,
  BACKLIGHT,
  UNUSED,
  UNKNOWN
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
  SPECULAR,
  SUBSURFACEPBR,
  UNKNOWN
};

auto getStrFromTexType(const TextureType &Type) -> std::string;

auto getTexTypeFromStr(const std::string &Type) -> TextureType;

auto getSlotFromTexType(const TextureType &Type) -> TextureSlots;

struct PGTexture {
  std::filesystem::path Path;
  TextureType Type{};

  // Equality operator
  auto operator==(const PGTexture& Other) const -> bool {
      return Path == Other.Path && Type == Other.Type;
  }
};

struct PGTextureHasher {
    auto operator()(const PGTexture& Texture) const -> size_t {
        // Hash the path and the texture type, and combine them
        std::size_t PathHash = std::hash<std::filesystem::path>()(Texture.Path);
        std::size_t TypeHash = std::hash<int>()(static_cast<int>(Texture.Type));

        // Combine the hashes using bitwise XOR and bit shifting
        return PathHash ^ (TypeHash << 1);
    }
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
auto getTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot) -> std::string;
auto getTextureSlots(nifly::NifFile &NIF, nifly::NiShape *NIFShape) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;
auto getTexBase(const std::filesystem::path &TexPath) -> std::wstring;
auto getTexMatch(const std::wstring &Base, const TextureType &DesiredType,
                 const std::map<std::wstring, std::unordered_set<PGTexture, PGTextureHasher>> &SearchMap) -> std::vector<PGTexture>;
// Gets all the texture prefixes for a textureset. ie. _n.dds is removed etc. for each slot
auto getSearchPrefixes(nifly::NifFile &NIF, nifly::NiShape *NIFShape)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

auto getSearchPrefixes(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

} // namespace NIFUtil
