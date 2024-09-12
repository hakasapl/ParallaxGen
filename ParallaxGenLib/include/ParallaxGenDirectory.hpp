#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <vector>

#include "NIFUtil.hpp"
#include "BethesdaDirectory.hpp"
#include "ParallaxGenTask.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
  std::array<std::map<std::wstring, NIFUtil::PGTexture>, NUM_TEXTURE_SLOTS> TextureMaps;
  std::vector<std::filesystem::path> Meshes;
  std::vector<std::filesystem::path> PBRJSONs;
  std::vector<std::filesystem::path> PGJSONs;

  // Mutexes
  std::mutex TextureMapsMutex;
  std::mutex MeshesMutex;

public:
  // constructor - calls the BethesdaDirectory constructor
  ParallaxGenDirectory(BethesdaGame BG);

  auto findPGFiles() -> void;

private:
  auto mapTexturesFromNIF(const std::filesystem::path &NIFPath, std::unordered_map<std::filesystem::path, std::unordered_set<NIFUtil::TextureSlots>> &UnconfirmedTextures, std::mutex &UnconfirmedTexturesMutex) -> ParallaxGenTask::PGResult;

  static auto updateUnconfirmedTextureMap(const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot, std::unordered_map<std::filesystem::path, std::unordered_set<NIFUtil::TextureSlots>> &UnconfirmedTextures, std::mutex &Mutex) -> void;

  auto addToTextureMaps(const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot) -> void;

  auto addMesh(const std::filesystem::path &Path) -> void;

public:
  [[nodiscard]] auto getTextureMap(const NIFUtil::TextureSlots &Slot) -> std::map<std::wstring, NIFUtil::PGTexture>&;

  [[nodiscard]] auto getTextureMapConst(const NIFUtil::TextureSlots &Slot) const -> const std::map<std::wstring, NIFUtil::PGTexture>&;

  [[nodiscard]] auto getMeshes() const -> const std::vector<std::filesystem::path>&;

  [[nodiscard]] auto getPBRJSONs() const -> const std::vector<std::filesystem::path>&;

  [[nodiscard]] auto getPGJSONs() const -> const std::vector<std::filesystem::path>&;
};
