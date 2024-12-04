#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <shlwapi.h>
#include <string>
#include <unordered_set>
#include <winnt.h>

#include "NIFUtil.hpp"
#include "Patchers/PatcherShader.hpp"

/**
 * @class PatcherComplexMaterial
 * @brief Shader patcher for complex material
 */
class PatcherComplexMaterial : public PatcherShader {
private:
  static std::vector<std::wstring> DynCubemapBlocklist; /** Stores the dynamic cubemap blocklist set */
  static bool DisableMLP; /** If true MLP should be replaced with CM */

public:
  /**
   * @brief Get the Factory object
   *
   * @return PatcherShader::PatcherShaderFactory factory object for this patcher
   */
  static auto getFactory() -> PatcherShader::PatcherShaderFactory;

  /**
   * @brief Load required statics for CM patcher
   *
   * @param DisableMLP If true MLP should be replaced with CM
   * @param DynCubemapBlocklist Set of blocklisted dynamic cubemaps
   */
  static void loadStatics(const bool &DisableMLP, const std::vector<std::wstring> &DynCubemapBlocklist);

  /**
   * @brief Get the shader type for this patcher (CM)
   *
   * @return NIFUtil::ShapeShader CM shader type
   */
  static auto getShaderType() -> NIFUtil::ShapeShader;

  /**
   * @brief Construct a new complex material patcher object
   *
   * @param NIFPath Path to NIF file
   * @param NIF NIF object to patch
   */
  PatcherComplexMaterial(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  /**
   * @brief Check if the shape can accomodate the CM shader (without looking at texture slots)
   *
   * @param NIFShape Shape to check
   * @return true Shape can accomodate CM
   * @return false Shape cannot accomodate CM
   */
  auto canApply(nifly::NiShape &NIFShape) -> bool override;

  /**
   * @brief Check if shape can accomodate CM shader based on texture slots only
   *
   * @param NIFShape Shape to check
   * @param Matches Vector of matches
   * @return true Match found
   * @return false No match found
   */
  auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool override;

  /**
   * @brief Check if slots can accomodate CM shader
   *
   * @param OldSlots Slots to check
   * @param Matches Vector of matches
   * @return true Match found
   * @return false No match found
   */
  auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                   std::vector<PatcherMatch> &Matches) -> bool override;

  /**
   * @brief Apply the CM shader to the shape
   *
   * @param[in] NIFShape Shape to apply to
   * @param[in] Match Match to apply
   * @param[out] NIFModified If NIF was modified
   * @param[out] ShapeDeleted If shape was deleted
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots after patching
   */
  auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                  bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  /**
   * @brief Apply the CM shader to the slots
   *
   * @param OldSlots Slots to apply to
   * @param Match Match to apply
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots after patching
   */
  auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                       const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  /**
   * @brief Apply neutral textures to slots
   *
   * @param Slots Slots to apply to
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots after patching
   */
  auto applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
      -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;
};
