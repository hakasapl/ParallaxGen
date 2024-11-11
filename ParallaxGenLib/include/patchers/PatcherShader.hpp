#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>

#include <vector>

#include "NIFUtil.hpp"
#include "patchers/Patcher.hpp"

class PatcherShader : public Patcher {
public:
  using PatcherShaderFactory = std::function<std::unique_ptr<PatcherShader>(std::filesystem::path, nifly::NifFile*)>;
  using PatcherShaderObject = std::unique_ptr<PatcherShader>;

  struct PatcherMatch {
    std::wstring MatchedPath;                              // The path of the matched file
    std::unordered_set<NIFUtil::TextureSlots> MatchedFrom; // The texture slots that the match matched with
    std::shared_ptr<void> ExtraData; // Any extra data the patcher might need intermally to do the patch
  };

  // Constructors
  PatcherShader(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName);
  virtual ~PatcherShader() = default;
  PatcherShader(const PatcherShader &Other) = default;
  auto operator=(const PatcherShader &Other) -> PatcherShader & = default;
  PatcherShader(PatcherShader &&Other) noexcept = default;
  auto operator=(PatcherShader &&Other) noexcept -> PatcherShader & = default;

  virtual auto canApply(nifly::NiShape &NIFShape) -> bool = 0;

  /// @brief  Methods that determine whether the patcher should apply to a shape
  /// @param[in] NIFShape shape to check
  /// @param Matches found matches
  /// @return if any match was found
  virtual auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool = 0;

  /// @brief determine if the patcher should be applied to the shape
  /// @param[in] OldSlots array of texture slot textures
  /// @param[out] Matches vector of matches for the given textures
  /// @return if any match was found
  virtual auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                           std::vector<PatcherMatch> &Matches) -> bool = 0;

  // Methods that apply the patch to a shape
  virtual auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                          bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> = 0;

  /// @brief apply the matched texture to the texture slots
  /// @param[in] OldSlots array of the slot textures
  /// @param[in] Match matching texture
  /// @return new array containing the applied matched texture
  virtual auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                               const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> = 0;

  /// @brief apply neutral textures according to the that don't change the appearance
  /// @param[in] OldSlots array of the slot textures
  /// @return new array containing the applied textures
  virtual auto applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
      -> std::array<std::wstring, NUM_TEXTURE_SLOTS> = 0;
};
