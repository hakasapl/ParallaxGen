#pragma once

#include <DirectXTex.h>
#include <NifFile.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <winnt.h>

#include "BethesdaDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"

#define MAPTEXTURE_PROGRESS_MODULO 10

class ParallaxGenDirectory : public BethesdaDirectory {
private:
  struct UnconfirmedTextureProperty {
    std::unordered_map<NIFUtil::TextureSlots, size_t> Slots;
    std::unordered_map<NIFUtil::TextureType, size_t> Types;
  };

  // Temp Structures
  std::unordered_map<std::filesystem::path, UnconfirmedTextureProperty> UnconfirmedTextures;
  std::mutex UnconfirmedTexturesMutex;
  std::unordered_set<std::filesystem::path> UnconfirmedMeshes;

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

  auto findFiles() -> void;
  auto mapFiles(const std::unordered_set<std::wstring> &NIFBlocklist,
                const std::unordered_map<std::filesystem::path, NIFUtil::TextureType> &ManualTextureMaps,
                const bool &MapFromMeshes = true, const bool &Multithreading = true, const bool &CacheNIFs = false) -> void;

private:
  auto mapTexturesFromNIF(const std::filesystem::path &NIFPath, const bool &CacheNIF = false) -> ParallaxGenTask::PGResult;

  auto updateUnconfirmedTexturesMap(
      const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot, const NIFUtil::TextureType &Type,
      std::unordered_map<std::filesystem::path, UnconfirmedTextureProperty> &UnconfirmedTextureSlots) -> void;

  auto addToTextureMaps(const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot,
                        const NIFUtil::TextureType &Type) -> void;

  auto addMesh(const std::filesystem::path &Path) -> void;

public:
  static auto checkGlobMatchInSet(const std::wstring &Check, const std::unordered_set<std::wstring> &List) -> bool;

  [[nodiscard]] auto getTextureMap(const NIFUtil::TextureSlots &Slot) -> std::map<std::wstring, NIFUtil::PGTexture> &;

  [[nodiscard]] auto
  getTextureMapConst(const NIFUtil::TextureSlots &Slot) const -> const std::map<std::wstring, NIFUtil::PGTexture> &;

  [[nodiscard]] auto getMeshes() const -> const std::unordered_set<std::filesystem::path> &;

  [[nodiscard]] auto getPBRJSONs() const -> const std::vector<std::filesystem::path> &;

  [[nodiscard]] auto getPGJSONs() const -> const std::vector<std::filesystem::path> &;
};
