#pragma once

#include <DirectXTex.h>
#include <NifFile.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

#include "BethesdaDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
  struct UnconfirmedTextureProperty {
    std::unordered_map<NIFUtil::TextureSlots, size_t> Slots;
    std::unordered_map<NIFUtil::TextureType, size_t> Types;
  };

  // Structures to store relevant files (sometimes their contents)
  std::array<std::map<std::wstring, NIFUtil::PGTexture>, NUM_TEXTURE_SLOTS> TextureMaps;
  std::unordered_set<std::filesystem::path> Meshes;
  std::vector<std::filesystem::path> PBRJSONs;
  std::vector<std::filesystem::path> PGJSONs;

  // Mutexes
  std::mutex TextureMapsMutex;
  std::mutex MeshesMutex;

public:
  // constructor - calls the BethesdaDirectory constructor
  ParallaxGenDirectory(BethesdaGame BG);

  auto findPGFiles(const bool &MapFromMeshes = true) -> void;

private:
  auto mapTexturesFromNIF(const std::filesystem::path &NIFPath,
                          std::unordered_map<std::filesystem::path, UnconfirmedTextureProperty> &UnconfirmedTextures,
                          std::mutex &UnconfirmedTexturesMutex) -> ParallaxGenTask::PGResult;

  static auto updateUnconfirmedTexturesMap(
      const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot, const NIFUtil::TextureType &Type,
      std::unordered_map<std::filesystem::path, UnconfirmedTextureProperty> &UnconfirmedTextureSlots,
      std::mutex &Mutex) -> void;

  auto addToTextureMaps(const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot,
                        const NIFUtil::TextureType &Type) -> void;

  auto addMesh(const std::filesystem::path &Path) -> void;

public:
  [[nodiscard]] auto getTextureMap(const NIFUtil::TextureSlots &Slot) -> std::map<std::wstring, NIFUtil::PGTexture> &;

  [[nodiscard]] auto
  getTextureMapConst(const NIFUtil::TextureSlots &Slot) const -> const std::map<std::wstring, NIFUtil::PGTexture> &;

  [[nodiscard]] auto getMeshes() const -> const std::unordered_set<std::filesystem::path> &;

  [[nodiscard]] auto getPBRJSONs() const -> const std::vector<std::filesystem::path> &;

  [[nodiscard]] auto getPGJSONs() const -> const std::vector<std::filesystem::path> &;
};
