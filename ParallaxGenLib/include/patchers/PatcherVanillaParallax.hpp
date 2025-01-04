#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <string>

#include "NIFUtil.hpp"
#include "Patchers/PatcherShader.hpp"

/**
 * @class PatcherVanillaParallax
 * @brief Patcher for vanilla parallax
 */
class PatcherVanillaParallax : public PatcherShader {
private:
  bool HasAttachedHavok = false;  /** Stores at the NIF level whether the NIF has attached havok (incompatible with parallax) */

public:
  /**
   * @brief Get the Factory object for parallax patcher
   *
   * @return PatcherShader::PatcherShaderFactory Factory object
   */
  static auto getFactory() -> PatcherShader::PatcherShaderFactory;

  /**
   * @brief Get the Shader Type for this patcher (Parallax)
   *
   * @return NIFUtil::ShapeShader Parallax
   */
  static auto getShaderType() -> NIFUtil::ShapeShader;

  /**
   * @brief Construct a new Patcher Vanilla Parallax object
   *
   * @param NIFPath NIF path to patch
   * @param NIF NIF object to patch
   */
  PatcherVanillaParallax(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  /**
   * @brief Check if a shape can be patched by this patcher (without looking at slots)
   *
   * @param NIFShape Shape to check
   * @return true Shape can be patched
   * @return false Shape cannot be patched
   */
  auto canApply(nifly::NiShape &NIFShape) -> bool override;

  /**
   * @brief Check if a shape can be patched by this patcher (with slots)
   *
   * @param NIFShape Shape to check
   * @param[out] Matches Matches found
   * @return true Found matches
   * @return false No matches found
   */
  auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool override;

  /**
   * @brief Check if slots can accomodate parallax
   *
   * @param OldSlots Slots to check
   * @param[out] Matches Matches found
   * @return true Found matches
   * @return false No matches found
   */
  auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                   std::vector<PatcherMatch> &Matches) -> bool override;

  /**
   * @brief Apply a match to a shape for parallax
   *
   * @param NIFShape Shape to patch
   * @param Match Match to apply
   * @param[out] NIFModified Whether the NIF was modified
   * @param[out] ShapeDeleted Whether the shape was deleted (always false)
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots of shape
   */
  auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                  bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  /**
   * @brief Apply a match to slots for parallax
   *
   * @param OldSlots Slots to patch
   * @param Match Match to apply
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots
   */
  auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                       const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  /**
   * @brief Apply parallax shader to a shape
   *
   * @param NIFShape Shape to apply shader to
   * @param NIFModified Whether the NIF was modified
   */
  void applyShader(nifly::NiShape &NIFShape, bool &NIFModified) override;

  /**
   * @brief Apply a neutral texture to slots for parallax
   *
   * @param Slots Slots to apply neutral tex to
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots
   */
  auto applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
      -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;
};
