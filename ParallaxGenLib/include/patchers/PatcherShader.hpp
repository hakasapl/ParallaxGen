#pragma once

#include <NifFile.hpp>

#include <vector>

#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"

class PatcherShader {
private:
  // Instance vars
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;
  std::string PatcherName;
  NIFUtil::ShapeShader ShaderType;

  // Static Tools
  static ParallaxGenDirectory *PGD;
  static ParallaxGenD3D *PGD3D;

protected:
  [[nodiscard]] auto getNIFPath() const -> std::filesystem::path;
  [[nodiscard]] auto getNIF() const -> nifly::NifFile *;

public:
  [[nodiscard]] auto getPatcherName() const -> std::string;
  [[nodiscard]] auto getShaderType() const -> NIFUtil::ShapeShader;

  struct PatcherMatch {
    std::wstring MatchedPath;                              // The path of the matched file
    std::unordered_set<NIFUtil::TextureSlots> MatchedFrom; // The texture slots that the match matched with
    std::shared_ptr<void> ExtraData; // Any extra data the patcher might need intermally to do the patch
  };

  // Constructors
  PatcherShader(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName,
                const NIFUtil::ShapeShader &ShaderType);
  virtual ~PatcherShader() = default;
  PatcherShader(const PatcherShader &Other) = default;
  auto operator=(const PatcherShader &Other) -> PatcherShader & = default;
  PatcherShader(PatcherShader &&Other) noexcept = default;
  auto operator=(PatcherShader &&Other) noexcept -> PatcherShader & = default;

  static void loadStatics(ParallaxGenDirectory &PGD, ParallaxGenD3D &PGD3D);

  // Common helpers for all patchers
  [[nodiscard]] static auto isSameAspectRatio(const std::filesystem::path &Texture1,
                                              const std::filesystem::path &Texture2) -> bool;
  [[nodiscard]] static auto isPrefixOrFile(const std::filesystem::path &Path) -> bool;
  [[nodiscard]] static auto isFile(const std::filesystem::path &File) -> bool;
  [[nodiscard]] static auto getFile(const std::filesystem::path &File) -> std::vector<std::byte>;
  [[nodiscard]] static auto getSlotLookupMap(const NIFUtil::TextureSlots &Slot)
      -> const std::map<std::wstring, std::unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> &;

  // Methods that determine whether the patcher should apply to a shape
  virtual auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool = 0;
  virtual auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                           std::vector<PatcherMatch> &Matches) -> bool = 0;

  // Methods that apply the patch to a shape
  virtual auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                          bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> = 0;
  virtual auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                               const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> = 0;
};
