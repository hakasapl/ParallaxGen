#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>
#include <Shaders.hpp>
#include <array>
#include <tuple>

constexpr unsigned NUM_TEXTURE_SLOTS = 9;

namespace NIFUtil {
// These need to be in the order of worst shader to best shader
enum class ShapeShader { NONE, VANILLAPARALLAX, COMPLEXMATERIAL, TRUEPBR };

/// @brief get a string that represents the given shader
/// @param[in] Shader shader type
/// @return string containing the name of the shader
auto getStrFromShader(const ShapeShader &Shader) -> std::string;

/// @brief zero-based index of texture in BSShaderTextureSet
/// there can be more than one type of textures assigned to a a texture slot, the slot name describes the default one
enum class TextureSlots : unsigned int {
  DIFFUSE,
  NORMAL,
  GLOW,
  PARALLAX,
  CUBEMAP,
  ENVMASK,
  MULTILAYER,
  BACKLIGHT,
  UNUSED,
  UNKNOWN
};

/// @brief All known types of textures
enum class TextureType {
  DIFFUSE,
  NORMAL,
  MODELSPACENORMAL,
  EMISSIVE,
  SKINTINT,
  SUBSURFACECOLOR,
  HEIGHT,
  CUBEMAP,
  ENVIRONMENTMASK,
  COMPLEXMATERIAL,
  RMAOS,
  SUBSURFACETINT,
  INNERLAYER,
  COATNORMALROUGHNESS,
  BACKLIGHT,
  SPECULAR,
  SUBSURFACEPBR,
  UNKNOWN
};

auto getStrFromTexType(const TextureType &Type) -> std::string;

auto getTexTypeFromStr(const std::string &Type) -> TextureType;

auto getSlotFromTexType(const TextureType &Type) -> TextureSlots;

/// @brief texture used by parallaxgen with type
struct PGTexture {
  /// @brief relative path in the data directory
  std::filesystem::path Path;
  TextureType Type{};

  // Equality operator
  auto operator==(const PGTexture &Other) const -> bool { return Path == Other.Path && Type == Other.Type; }
};

struct PGTextureHasher {
  auto operator()(const PGTexture &Texture) const -> size_t {
    // Hash the path and the texture type, and combine them
    std::size_t PathHash = std::hash<std::filesystem::path>()(Texture.Path);
    std::size_t TypeHash = std::hash<int>()(static_cast<int>(Texture.Type));

    // Combine the hashes using bitwise XOR and bit shifting
    return PathHash ^ (TypeHash << 1);
  }
};

/// @brief get the texture type that is assigned per default to a texture slot
/// @param[in] Slot
/// @return texture type
auto getDefaultTextureType(const TextureSlots &Slot) -> TextureType;

/// @brief load a Nif from memory
/// @param[in] NIFBytes memory containing the NIF
/// @return the nif
auto loadNIFFromBytes(const std::vector<std::byte> &NIFBytes) -> nifly::NifFile;

/// @brief get a map containing the known texture suffixes
/// @return the map containing the suffixes and the slot/type pairs
auto getTexSuffixMap() -> std::map<std::wstring, std::tuple<TextureSlots, TextureType>>;

/// @brief Deduct the texture type and slot usually used from the suffix of a texture
/// @param[in] Path texture to check
/// @return pair of texture slot and type of that texture
auto getDefaultsFromSuffix(const std::filesystem::path &Path) -> std::tuple<TextureSlots, TextureType>;

/// @brief set the shader type of a given shader
/// @param NIFShader the shader
/// @param[in] Type type that is set
/// @param[out] Changed false if the type was already set, true otherwise
auto setShaderType(nifly::NiShader *NIFShader, const nifly::BSLightingShaderPropertyShaderType &Type,
                   bool &Changed) -> void;

/// @brief set a new value for a float value
/// @param Value the value to change
/// @param[in] NewValue the new value to set
/// @param Changed if the value was changed
auto setShaderFloat(float &Value, const float &NewValue, bool &Changed) -> void;

/// @brief set a new value for a vector float value
/// @param Value the value to change
/// @param[in] NewValue the new value
/// @param[out] Changed if the value was actually changed
/// @return
auto setShaderVec2(nifly::Vector2 &Value, const nifly::Vector2 &NewValue, bool &Changed) -> void;

/// @brief check if a given flag is set for a shader
/// @param NIFShaderBSLSP the shader to check
/// @param[in] Flag the flag to check
/// @return if the flag is set
auto hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag) -> bool;

/// @brief check if a given flag is set for a shader
/// @param NIFShaderBSLSP the shader to check
/// @param[in] Flag the flag to check
/// @return if the flag is set
auto hasShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag) -> bool;

/// @brief set a given shader flag 1 for a shader
/// @param NIFShaderBSLSP the shader
/// @param[in] Flag the flag to set
/// @param[out] Changed false if flag was already set
auto setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                   bool &Changed) -> void;

/// @brief set a given shader flag 2 for a shader
/// @param NIFShaderBSLSP the shader
/// @param[in] Flag the flag to set
/// @param[out] Changed false if flag was already set
auto setShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                   bool &Changed) -> void;

/// @brief clear all shader flags 1 for a shader
/// @param NIFShaderBSLSP the shader
/// @param[in] Flag the flag1 to clear
/// @param[out] Changed false if flags was already empty
auto clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                     bool &Changed) -> void;

/// @brief clear all shader flags 2 for a shader
/// @param NIFShaderBSLSP the shader
/// @param[in] Flag the flag2 to clear
/// @param[out] Changed false if flags were already empty
auto clearShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                     bool &Changed) -> void;


/// @brief set or unset a given shader flag 1 for a shader
/// @param NIFShaderBSLSP the shader
/// @param[in] Flag the flag to set
/// @param[in] Enable enable or disable the flag
/// @param[out] Changed if flag was changed during the operation
auto configureShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1 &Flag,
                         const bool &Enable, bool &Changed) -> void;

/// @brief set or unset a given shader flag 2 for a shader
/// @param NIFShaderBSLSP the shader
/// @param[in] Flag the flag to set
/// @param[in] Enable enable or disable the flag
/// @param[out] Changed if flag was changed during the operation
auto configureShaderFlag(nifly::BSShaderProperty *NIFShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2 &Flag,
                         const bool &Enable, bool &Changed) -> void;


/// @brief set the path of a texture slot for a shape, unicode variant
/// @param NIF nif
/// @param NIFShape the shape
/// @param[in] Slot texture slot
/// @param[in] TexturePath the path to set
/// @param[out] Changed false if the path was already set
auto setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot,
                    const std::wstring &TexturePath, bool &Changed) -> void;

/// @brief set the path of a texture slot for a shape
/// @param NIF nif
/// @param NIFShape the shape
/// @param[in] Slot texture slot
/// @param[in] TexturePath the path to set
/// @param[out] Changed false if the path was already set
auto setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot,
                    const std::string &TexturePath, bool &Changed) -> void;

/// @brief set all paths of a texture slot for a shape
/// @param NIF nif
/// @param NIFShape the shape
/// @param[in] NewSlots textures to set
/// @param Changed false if none of the slots was changed
auto setTextureSlots(nifly::NifFile *NIF, nifly::NiShape *NIFShape,
                     const std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots, bool &Changed) -> void;

/// @brief get the set texture for a slot
/// @param NIF nif
/// @param NIFShape the shape
/// @param[in] Slot the slot
/// @return texture set in the slot
auto getTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot) -> std::string;

/// @brief get all set texture slots from a shape
/// @param NIF nif
/// @param NIFShape shape
/// @return array of textures set in the slots
auto getTextureSlots(nifly::NifFile *NIF, nifly::NiShape *NIFShape) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

/// @brief get the texture name without suffix, i.e. without _n.dds
/// @param[in] TexPath the path to get the base for
/// @return base path
auto getTexBase(const std::filesystem::path &TexPath) -> std::wstring;

/// @brief get the matching textures for a given base path
/// @param[in] Base base texture name
/// @param[in] DesiredType the type to find
/// @param[in] SearchMap base names without suffix mapped to a set of potential textures. strings and paths must all be lowercase
/// @return vector of textures
auto getTexMatch(const std::wstring &Base, const TextureType &DesiredType,
                 const std::map<std::wstring, std::unordered_set<PGTexture, PGTextureHasher>> &SearchMap)
    -> std::vector<PGTexture>;

/// @brief Gets all the texture prefixes for a textureset from a nif shape, ie. _n.dds is removed etc. for each slot
/// @param[in] NIF the nif
/// @param NIFShape the shape
/// @return array of texture names without suffixes
auto getSearchPrefixes(nifly::NifFile const& NIF, nifly::NiShape *NIFShape) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

/// @brief Gets all the texture prefixes for a texture set. ie. _n.dds is removed etc. for each slot
/// @param[in] OldSlots
/// @return array of texture names without suffixes
auto getSearchPrefixes(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

} // namespace NIFUtil
