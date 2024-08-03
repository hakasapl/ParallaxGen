#pragma once

#include "Defs.hpp"
#include <Geometry.hpp>
#include <NifFile.hpp>
#include <Shaders.hpp>

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
auto setTextureSlot(nifly::NifFile *NIF, nifly::NiShape *NIFShape, const TextureSlots &Slot,
                    const std::string &TexturePath, bool &Changed) -> void;
// Gets all the texture prefixes for a textureset. ie. _n.dds is removed etc. for each slot
auto getSearchPrefixes(nifly::NifFile &NIF, nifly::NiShape *NIFShape) -> std::array<std::string, NUM_TEXTURE_SLOTS>;

} // namespace NIFUtil
