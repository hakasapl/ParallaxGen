#pragma once

#include <mutex>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

#include "BethesdaGame.hpp"
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"

// Forward declaration
class ParallaxGenDirectory;

class ParallaxGenConfig {
public:
  struct PGParams {
    // CLI Only
    bool Autostart = false;

    // Game
    struct Game {
      std::filesystem::path Dir;
      BethesdaGame::GameType Type = BethesdaGame::GameType::SKYRIM_SE;
    } Game;

    // Mod Manager
    struct ModManager {
      ModManagerDirectory::ModManagerType Type = ModManagerDirectory::ModManagerType::None;
      std::filesystem::path MO2InstanceDir;
      std::wstring MO2Profile = L"Default";
    } ModManager;

    // Output
    struct Output {
      std::filesystem::path Dir;
      bool Zip = true;
    } Output;

    // Processing
    struct Processing {
      bool Multithread = true;
      bool HighMem = false;
      bool GPUAcceleration = true;
      bool BSA = true;
      bool PluginPatching = true;
      bool MapFromMeshes = true;
    } Processing;

    // Pre-Patchers
    struct PrePatcher {
      bool DisableMLP = false;
    } PrePatcher;

    // Shader Patchers
    struct ShaderPatcher {
      bool Parallax = true;
      bool ComplexMaterial = true;
      bool TruePBR = true;
    } ShaderPatcher;

    // Shader Transforms
    struct ShaderTransforms {
      bool ParallaxToCM = false;
    } ShaderTransforms;

    // Post-Patchers
    struct PostPatcher {
      bool OptimizeMeshes = false;
    } PostPatcher;

    [[nodiscard]] auto getString() const -> std::wstring;
  };

private:
  std::filesystem::path ExePath;

  // Config Structures
  std::unordered_set<std::wstring> NIFBlocklist;
  std::unordered_set<std::wstring> DynCubemapBlocklist;
  std::unordered_map<std::filesystem::path, NIFUtil::TextureType> ManualTextureMaps;
  std::unordered_set<std::wstring> VanillaBSAList;
  PGParams Params;

  std::mutex ModOrderMutex;
  std::vector<std::wstring> ModOrder;
  std::unordered_map<std::wstring, int> ModPriority;

  // Validator
  nlohmann::json_schema::json_validator Validator;

public:
  ParallaxGenConfig(std::filesystem::path ExePath);
  static auto getConfigValidation() -> nlohmann::json;
  auto getUserConfigFile() const -> std::filesystem::path;

  void loadConfig();

  [[nodiscard]] auto getNIFBlocklist() const -> const std::unordered_set<std::wstring> &;

  [[nodiscard]] auto getDynCubemapBlocklist() const -> const std::unordered_set<std::wstring> &;

  [[nodiscard]] auto
  getManualTextureMaps() const -> const std::unordered_map<std::filesystem::path, NIFUtil::TextureType> &;

  [[nodiscard]] auto getVanillaBSAList() const -> const std::unordered_set<std::wstring> &;

  [[nodiscard]] auto getModOrder() -> std::vector<std::wstring>;
  [[nodiscard]] auto getModPriority(const std::wstring &Mod) -> int;
  [[nodiscard]] auto getModPriorityMap() -> std::unordered_map<std::wstring, int>;
  void setModOrder(const std::vector<std::wstring> &ModOrder);

  [[nodiscard]] auto getParams() const -> PGParams;
  void setParams(const PGParams &Params);
  [[nodiscard]] static auto validateParams(const PGParams &Params, std::vector<std::string> &Errors) -> bool;

private:
  static auto parseJSON(const std::filesystem::path &JSONFile, const std::vector<std::byte> &Bytes,
                        nlohmann::json &J) -> bool;

  auto validateJSON(const std::filesystem::path &JSONFile, const nlohmann::json &J) -> bool;

  auto addConfigJSON(const nlohmann::json &J) -> void;

  static void replaceForwardSlashes(nlohmann::json &JSON);

  void saveUserConfig() const;
};
