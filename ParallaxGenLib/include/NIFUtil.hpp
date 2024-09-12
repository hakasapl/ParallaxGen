#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>
#include <Shaders.hpp>
#include <array>

#define NUM_TEXTURE_SLOTS 9

namespace NIFUtil {
enum class TextureSlots : unsigned int {
  Diffuse = 0,
  Normal = 1,
  Glow = 2,
  Parallax = 3,
  Cubemap = 4,
  EnvMask = 5,
  Tint = 6,
  Backlight = 7,
  Unused = 8
};

enum class PGTextureType {
  DIFFUSE,
  NORMAL,
  GLOW,
  HEIGHT,
  CUBEMAP,
  ENVIRONMENTMASK,
  COMPLEXMATERIAL,
  RMAOS,
  TINT,
  COATNORMAL,
  BACKLIGHT
};

struct PGTexture {
  std::filesystem::path Path;
  PGTextureType Type;
};

auto getDefaultTextureType(const TextureSlots &Slot) -> PGTextureType;

// TODO this should be in UTIL
// Custom comparator for comparing strings from right to left
struct ReverseComparator {
  template <typename StringT>
  auto operator()(const StringT& Lhs, const StringT& Rhs) const -> bool {
    auto LhsIt = Lhs.rbegin();
    auto RhsIt = Rhs.rbegin();

    while (LhsIt != Lhs.rend() && RhsIt != Rhs.rend()) {
      if (*LhsIt != *RhsIt) {
          return *LhsIt < *RhsIt;
      }
      ++LhsIt;
      ++RhsIt;
    }
    return Lhs.size() < Rhs.size();
  }
};

auto loadNIFFromBytes(const std::vector<std::byte> &NIFBytes) -> nifly::NifFile;

auto getTexSuffixMap() -> std::map<std::wstring, TextureSlots, ReverseComparator>;

auto getSlotFromPath(const std::filesystem::path &Path) -> TextureSlots;

// shader helpers
auto setShaderType(nifly::NiShader *NIFShader, const nifly::BSLightingShaderPropertyShaderType &Type,
                   bool &Changed) -> void;
auto setShaderFloat(float &Value, const float &NewValue, bool &Changed) -> void;
auto setShaderVec2(nifly::Vector2 &Value, const nifly::Vector2 &NewValue, bool &Changed) -> void;

// Shader flag helpers
auto hasShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                   const nifly::SkyrimShaderPropertyFlags1 &Flag) -> bool;
auto hasShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP,
                   const nifly::SkyrimShaderPropertyFlags2 &Flag) -> bool;
auto setShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                   bool &Changed) -> void;
auto setShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                   bool &Changed) -> void;
auto clearShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                     bool &Changed) -> void;
auto clearShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                     bool &Changed) -> void;
auto configureShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                         const bool &Enable, bool &Changed) -> void;
auto configureShaderFlag(nifly::BSLightingShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                         const bool &Enable, bool &Changed) -> void;

// Texture slot helpers
auto setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot, const std::wstring &TexturePath, bool &Changed) -> void;
auto setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot,
                    const std::string &TexturePath, bool &Changed) -> void;
auto getTexBase(const std::filesystem::path &TexPath) -> std::wstring;
auto getTexBase(const std::wstring &TexPath) -> std::wstring;
auto getTexMatch(const std::wstring &Base,
                 const std::map<std::wstring, PGTexture> &SearchMap) -> PGTexture;
// Gets all the texture prefixes for a textureset. ie. _n.dds is removed etc. for each slot
auto getSearchPrefixes(nifly::NifFile &NIF, nifly::NiShape *NIFShape)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

} // namespace NIFUtil
