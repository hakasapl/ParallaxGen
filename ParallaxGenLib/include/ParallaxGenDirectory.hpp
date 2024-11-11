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
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"

class ModManagerDirectory;

#define MAPTEXTURE_PROGRESS_MODULO 10

class ParallaxGenDirectory : public BethesdaDirectory {
private:
  struct UnconfirmedTextureProperty {
    std::unordered_map<NIFUtil::TextureSlots, size_t> Slots;
    std::unordered_map<NIFUtil::TextureType, size_t> Types;
  };

  // Temp Structures
  std::unordered_map<std::filesystem::path, UnconfirmedTextureProperty> UnconfirmedTextures{};
  std::mutex UnconfirmedTexturesMutex{};
  std::unordered_set<std::filesystem::path> UnconfirmedMeshes{};

  // Structures to store relevant files (sometimes their contents)
  std::array<std::map<std::wstring, std::unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>>, NUM_TEXTURE_SLOTS> TextureMaps{};
  std::unordered_map<std::filesystem::path, NIFUtil::TextureType> TextureTypes;
  std::unordered_set<std::filesystem::path> Meshes{};
  std::vector<std::filesystem::path> PBRJSONs{};
  std::vector<std::filesystem::path> PGJSONs{};

  // Mutexes
  std::mutex TextureMapsMutex;
  std::mutex TextureTypesMutex;
  std::mutex MeshesMutex;

public:
  // constructor - calls the BethesdaDirectory constructor
  ParallaxGenDirectory(BethesdaGame BG, std::filesystem::path OutputPath = "", ModManagerDirectory *MMD = nullptr);

  /// @brief Map all files in the load order to their type
  ///
  /// @param NIFBlocklist Nifs to ignore for populating the mesh list
  /// @param ManualTextureMaps Overwrite the type of a texture
  /// @param ParallaxBSAExcludes Parallax maps contained in any of these BSAs are not considered for the file map
  /// @param MapFromMeshes The texture type is deducted from the shader/texture set it is assigned to, if false use only file suffix to determine type
  /// @param Multithreading Speedup mapping by multithreading
  /// @param CacheNIFs Faster but higher memory consumption
  auto mapFiles(const std::unordered_set<std::wstring> &NIFBlocklist,
                const std::unordered_map<std::filesystem::path, NIFUtil::TextureType> &ManualTextureMaps,
                const std::unordered_set<std::wstring> &ParallaxBSAExcludes,
                const bool &MapFromMeshes = true, const bool &Multithreading = true, const bool &CacheNIFs = false) -> void;

private:
  auto findFiles() -> void;

  auto mapTexturesFromNIF(const std::filesystem::path &NIFPath, const bool &CacheNIF = false) -> ParallaxGenTask::PGResult;

  auto updateUnconfirmedTexturesMap(
      const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot, const NIFUtil::TextureType &Type,
      std::unordered_map<std::filesystem::path, UnconfirmedTextureProperty> &UnconfirmedTextureSlots) -> void;

  auto addToTextureMaps(const std::filesystem::path &Path, const NIFUtil::TextureSlots &Slot,
                        const NIFUtil::TextureType &Type) -> void;

  auto addMesh(const std::filesystem::path &Path) -> void;

public:
  static auto checkGlobMatchInSet(const std::wstring &Check, const std::unordered_set<std::wstring> &List) -> bool;

  /// @brief Get the texture map for a given texture slot
  ///
  /// To populate the map call populateFileMap() and mapFiles().
  ///
  /// The key is the texture path without the suffix, the value is a set of texture paths.
  /// There can be more than one textures for a name without the suffix, since there are more than one possible suffixes
  /// for certain texture slots. Full texture paths are stored in each item of the value set.
  ///
  /// The decision between the two is handled later in the patching step. This ensures a _m doesn�t get replaced with an
  /// _em in the mesh for example (if both are cm) Because usually the existing thing in the slot is what is wanted if
  /// there are 2 or more possible options.
  ///
  /// Entry example:
  /// textures\\landscape\\dirtcliffs\\dirtcliffs01 -> {textures\\landscape\\dirtcliffs\\dirtcliffs01_mask.dds,
  /// textures\\landscape\\dirtcliffs\\dirtcliffs01.dds}
  ///
  /// @param Slot texture slot of BSShaderTextureSet in the shapes
  /// @return The mutable map
  [[nodiscard]] auto getTextureMap(const NIFUtil::TextureSlots &Slot) -> std::map<std::wstring, std::unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> &;

  /// @brief Get the immutable texture map for a given texture slot
  /// @see getTextureMap
  /// @param Slot texture slot of BSShaderTextureSet in the shapes
  /// @return The immutable map
  [[nodiscard]] auto
  getTextureMapConst(const NIFUtil::TextureSlots &Slot) const -> const std::map<std::wstring, std::unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> &;

  [[nodiscard]] auto getMeshes() const -> const std::unordered_set<std::filesystem::path> &;

  [[nodiscard]] auto getPBRJSONs() const -> const std::vector<std::filesystem::path> &;

  [[nodiscard]] auto getPGJSONs() const -> const std::vector<std::filesystem::path> &;

  auto getTextureType(const std::filesystem::path &Path) -> NIFUtil::TextureType;
};
