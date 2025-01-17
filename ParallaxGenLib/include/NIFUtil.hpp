#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>
#include <Shaders.hpp>
#include <array>
#include <tuple>

constexpr unsigned NUM_TEXTURE_SLOTS = 9;

namespace NIFUtil {
using TextureSet = std::array<std::wstring, NUM_TEXTURE_SLOTS>;

static constexpr float MIN_FLOAT_COMPARISON = 10e-05F;

// These need to be in the order of worst shader to best shader
enum class ShapeShader { NONE, UNKNOWN, VANILLAPARALLAX, COMPLEXMATERIAL, TRUEPBR };

/// @brief get a string that represents the given shader
/// @param[in] shader shader type
/// @return string containing the name of the shader
auto getStrFromShader(const ShapeShader& shader) -> std::string;

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
    HEIGHTPBR,
    CUBEMAP,
    ENVIRONMENTMASK,
    COMPLEXMATERIAL,
    RMAOS,
    SUBSURFACETINT,
    INNERLAYER,
    FUZZPBR,
    COATNORMALROUGHNESS,
    BACKLIGHT,
    SPECULAR,
    SUBSURFACEPBR,
    UNKNOWN
};

enum class TextureAttribute { CM_ENVMASK, CM_GLOSSINESS, CM_METALNESS, CM_HEIGHT };

auto getStrFromTexType(const TextureType& type) -> std::string;

auto getTexTypeFromStr(const std::string& type) -> TextureType;

auto getSlotFromTexType(const TextureType& type) -> TextureSlots;

/// @brief texture used by parallaxgen with type
struct PGTexture {
    /// @brief relative path in the data directory
    std::filesystem::path path;
    TextureType type {};

    // Equality operator
    auto operator==(const PGTexture& other) const -> bool { return path == other.path && type == other.type; }
};

struct PGTextureHasher {
    auto operator()(const PGTexture& texture) const -> size_t
    {
        // Hash the path and the texture type, and combine them
        std::size_t pathHash = std::hash<std::filesystem::path>()(texture.path);
        std::size_t typeHash = std::hash<int>()(static_cast<int>(texture.type));

        // Combine the hashes using bitwise XOR and bit shifting
        return pathHash ^ (typeHash << 1);
    }
};

auto getTexTypesStr() -> std::vector<std::string>;

/// @brief get the texture type that is assigned per default to a texture slot
/// @param[in] slot
/// @return texture type
auto getDefaultTextureType(const TextureSlots& slot) -> TextureType;

/// @brief load a Nif from memory
/// @param[in] nifBytes memory containing the NIF
/// @return the nif
auto loadNIFFromBytes(const std::vector<std::byte>& nifBytes) -> nifly::NifFile;

/// @brief get a map containing the known texture suffixes
/// @return the map containing the suffixes and the slot/type pairs
auto getTexSuffixMap() -> std::map<std::wstring, std::tuple<TextureSlots, TextureType>>;

/// @brief Deduct the texture type and slot usually used from the suffix of a texture
/// @param[in] path texture to check
/// @return pair of texture slot and type of that texture
auto getDefaultsFromSuffix(const std::filesystem::path& path) -> std::tuple<TextureSlots, TextureType>;

/// @brief set the shader type of a given shader
/// @param nifShader the shader
/// @param[in] type type that is set
/// @param[out] changed false if the type was already set, true otherwise
auto setShaderType(
    nifly::NiShader* nifShader, const nifly::BSLightingShaderPropertyShaderType& type, bool& changed) -> void;

/// @brief set a new value for a float value
/// @param value the value to change
/// @param[in] newValue the new value to set
/// @param[in,out] changed set to true if the value was changed, not altered otherwise
auto setShaderFloat(float& value, const float& newValue, bool& changed) -> void;

/// @brief set a new value for a vector float value
/// @param value the value to change
/// @param[in] newValue the new value
/// @param[in,out] changed set to true if the value was actually changed, not altered otherwise
/// @return
auto setShaderVec2(nifly::Vector2& value, const nifly::Vector2& newValue, bool& changed) -> void;

/// @brief check if a given flag is set for a shader
/// @param nifShaderBSLSP the shader to check
/// @param[in] flag the flag to check
/// @return if the flag is set
auto hasShaderFlag(nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1& flag) -> bool;

/// @brief check if a given flag is set for a shader
/// @param nifShaderBSLSP the shader to check
/// @param[in] flag the flag to check
/// @return if the flag is set
auto hasShaderFlag(nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2& flag) -> bool;

/// @brief set a given shader flag 1 for a shader
/// @param nifShaderBSLSP the shader
/// @param[in] flag the flag to set
/// @param[in,out] changed set to true if the flag actually changed, not altered otherwise
auto setShaderFlag(
    nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1& flag, bool& changed) -> void;

/// @brief set a given shader flag 2 for a shader
/// @param nifShaderBSLSP the shader
/// @param[in] flag the flag to set
/// @param[in,out] changed set to true if the flag actually changed, not altered otherwise
auto setShaderFlag(
    nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2& flag, bool& changed) -> void;

/// @brief clear all shader flags 1 for a shader
/// @param nifShaderBSLSP the shader
/// @param[in] flag the flag1 to clear
/// @param[in,out] changed set to true if the shader flags changed, not altered otherwise
auto clearShaderFlag(
    nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1& flag, bool& changed) -> void;

/// @brief clear all shader flags 2 for a shader
/// @param nifShaderBSLSP the shader
/// @param[in] flag the flag2 to clear
/// @param[in,out] changed set to true if the shader flags changed, not altered otherwise
auto clearShaderFlag(
    nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2& flag, bool& changed) -> void;

/// @brief set or unset a given shader flag 1 for a shader
/// @param nifShaderBSLSP the shader
/// @param[in] flag the flag to set
/// @param[in] enable enable or disable the flag
/// @param[in,out] changed set to true if flag was changed during the operation, not altered otherwise
auto configureShaderFlag(nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags1& flag,
    const bool& enable, bool& changed) -> void;

/// @brief set or unset a given shader flag 2 for a shader
/// @param nifShaderBSLSP the shader
/// @param[in] flag the flag to set
/// @param[in] enable enable or disable the flag
/// @param[in,out] changed set to true if the flag was changed, not altered otherwise
auto configureShaderFlag(nifly::BSShaderProperty* nifShaderBSLSP, const nifly::SkyrimShaderPropertyFlags2& flag,
    const bool& enable, bool& changed) -> void;

/// @brief set the path of a texture slot for a shape, unicode variant
/// @param nif nif
/// @param nifShape the shape
/// @param[in] slot texture slot
/// @param[in] texturePath the path to set
/// @param[in,out] changed set to true of the texture slot changed, not altered otherwise
auto setTextureSlot(nifly::NifFile* nif, nifly::NiShape* nifShape, const TextureSlots& slot,
    const std::wstring& texturePath, bool& changed) -> void;

/// @brief set the path of a texture slot for a shape
/// @param nif nif
/// @param nifShape the shape
/// @param[in] slot texture slot
/// @param[in] texturePath the path to set
/// @param[in,out] changed set to true if the slot changed, not altered otherwise
auto setTextureSlot(nifly::NifFile* nif, nifly::NiShape* nifShape, const TextureSlots& slot,
    const std::string& texturePath, bool& changed) -> void;

/// @brief set all paths of a texture slot for a shape
/// @param nif nif
/// @param nifShape the shape
/// @param[in] newSlots textures to set
/// @param[in,out] changed set to true if the slots changed, not altered otherwise
auto setTextureSlots(nifly::NifFile* nif, nifly::NiShape* nifShape,
    const std::array<std::wstring, NUM_TEXTURE_SLOTS>& newSlots, bool& changed) -> void;

/// @brief get the set texture for a slot
/// @param nif nif
/// @param nifShape the shape
/// @param[in] slot the slot
/// @return texture set in the slot
auto getTextureSlot(nifly::NifFile* nif, nifly::NiShape* nifShape, const TextureSlots& slot) -> std::string;

/// @brief get all set texture slots from a shape
/// @param nif nif
/// @param nifShape shape
/// @return array of textures set in the slots
auto getTextureSlots(nifly::NifFile* nif, nifly::NiShape* nifShape) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

/// @brief get the texture name without suffix, i.e. without _n.dds
/// @param[in] texPath the path to get the base for
/// @return base path
auto getTexBase(const std::filesystem::path& texPath) -> std::wstring;

/// @brief get the matching textures for a given base path
/// @param[in] base base texture name
/// @param[in] desiredType the type to find
/// @param[in] searchMap base names without suffix mapped to a set of potential textures. strings and paths must all be
/// lowercase
/// @return vector of textures
auto getTexMatch(const std::wstring& base, const TextureType& desiredType,
    const std::map<std::wstring, std::unordered_set<PGTexture, PGTextureHasher>>& searchMap) -> std::vector<PGTexture>;

/// @brief Gets all the texture prefixes for a textureset from a nif shape, ie. _n.dds is removed etc. for each slot
/// @param[in] nif the nif
/// @param nifShape the shape
/// @return array of texture names without suffixes
auto getSearchPrefixes(
    nifly::NifFile const& nif, nifly::NiShape* nifShape) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

/// @brief Gets all the texture prefixes for a texture set. ie. _n.dds is removed etc. for each slot
/// @param[in] oldSlots
/// @return array of texture names without suffixes
auto getSearchPrefixes(
    const std::array<std::wstring, NUM_TEXTURE_SLOTS>& oldSlots) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

} // namespace NIFUtil
