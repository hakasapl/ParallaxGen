#pragma once

#include <mutex>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

#include "BethesdaGame.hpp"
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"

/**
 * @class ParallaxGenConfig
 * @brief Class responsible for ParallaxGen runtime configuration and JSON file interaction
 */
class ParallaxGenConfig {
public:
  /**
   * @struct PGParams
   * @brief Struct that holds all the user-configurable parameters for ParallaxGen
   */
  struct PGParams {
    // CLI Only
    bool Autostart = false;

    // Game
    struct Game {
      std::filesystem::path Dir;
      BethesdaGame::GameType Type = BethesdaGame::GameType::SKYRIM_SE;

      auto operator==(const Game &Other) const -> bool { return Dir == Other.Dir && Type == Other.Type; }
    } Game;

    // Mod Manager
    struct ModManager {
      ModManagerDirectory::ModManagerType Type = ModManagerDirectory::ModManagerType::None;
      std::filesystem::path MO2InstanceDir;
      std::wstring MO2Profile = L"Default";
      bool MO2UseOrder = false;

      auto operator==(const ModManager &Other) const -> bool {
        return Type == Other.Type && MO2InstanceDir == Other.MO2InstanceDir && MO2Profile == Other.MO2Profile &&
               MO2UseOrder == Other.MO2UseOrder;
      }
    } ModManager;

    // Output
    struct Output {
      std::filesystem::path Dir;
      bool Zip = true;

      auto operator==(const Output &Other) const -> bool { return Dir == Other.Dir && Zip == Other.Zip; }
    } Output;

    // Advanced
    bool Advanced = false;

    // Processing
    struct Processing {
      bool Multithread = true;
      bool HighMem = false;
      bool GPUAcceleration = true;
      bool BSA = true;
      bool PluginPatching = true;
      bool PluginESMify = false;
      bool MapFromMeshes = true;

      auto operator==(const Processing &Other) const -> bool {
        return Multithread == Other.Multithread && HighMem == Other.HighMem &&
               GPUAcceleration == Other.GPUAcceleration && BSA == Other.BSA && PluginPatching == Other.PluginPatching &&
               MapFromMeshes == Other.MapFromMeshes;
      }
    } Processing;

    // Pre-Patchers
    struct PrePatcher {
      bool DisableMLP = false;

      auto operator==(const PrePatcher &Other) const -> bool { return DisableMLP == Other.DisableMLP; }
    } PrePatcher;

    // Shader Patchers
    struct ShaderPatcher {
      bool Parallax = true;
      bool ComplexMaterial = true;

      struct ShaderPatcherComplexMaterial {
        std::vector<std::wstring> ListsDyncubemapBlocklist;

        auto operator==(const ShaderPatcherComplexMaterial &Other) const -> bool {
          return ListsDyncubemapBlocklist == Other.ListsDyncubemapBlocklist;
        }
      } ShaderPatcherComplexMaterial;

      bool TruePBR = true;

      auto operator==(const ShaderPatcher &Other) const -> bool {
        return Parallax == Other.Parallax && ComplexMaterial == Other.ComplexMaterial && TruePBR == Other.TruePBR &&
               ShaderPatcherComplexMaterial == Other.ShaderPatcherComplexMaterial;
      }
    } ShaderPatcher;

    // Shader Transforms
    struct ShaderTransforms {
      bool ParallaxToCM = false;

      auto operator==(const ShaderTransforms &Other) const -> bool { return ParallaxToCM == Other.ParallaxToCM; }
    } ShaderTransforms;

    // Post-Patchers
    struct PostPatcher {
      bool OptimizeMeshes = false;

      auto operator==(const PostPatcher &Other) const -> bool { return OptimizeMeshes == Other.OptimizeMeshes; }
    } PostPatcher;

    // Lists
    struct MeshRules {
      std::vector<std::wstring> AllowList;
      std::vector<std::wstring> BlockList;

      auto operator==(const MeshRules &Other) const -> bool {
        return AllowList == Other.AllowList && BlockList == Other.BlockList;
      }
    } MeshRules;

    struct TextureRules {
      std::vector<std::wstring> VanillaBSAList;
      std::vector<std::pair<std::wstring, NIFUtil::TextureType>> TextureMaps;

      auto operator==(const TextureRules &Other) const -> bool {
        return VanillaBSAList == Other.VanillaBSAList && TextureMaps == Other.TextureMaps;
      }
    } TextureRules;

    [[nodiscard]] auto getString() const -> std::wstring;

    auto operator==(const PGParams &Other) const -> bool {
      return Autostart == Other.Autostart && Game == Other.Game && ModManager == Other.ModManager &&
             Output == Other.Output && Processing == Other.Processing && PrePatcher == Other.PrePatcher &&
             ShaderPatcher == Other.ShaderPatcher && ShaderTransforms == Other.ShaderTransforms &&
             PostPatcher == Other.PostPatcher && MeshRules == Other.MeshRules && TextureRules == Other.TextureRules &&
             Advanced == Other.Advanced;
    }

    auto operator!=(const PGParams &Other) const -> bool { return !(*this == Other); }
  };

private:
  std::filesystem::path ExePath; /** Stores the ExePath of ParallaxGen.exe */

  PGParams Params; /** Stores the configured parameters */

  std::mutex ModOrderMutex;           /** Mutex for locking mod order */
  std::vector<std::wstring> ModOrder; /** Stores the mod order configuration */
  std::unordered_map<std::wstring, int>
      ModPriority; /** Stores the priority numbering for mods for constant time lookups */

  nlohmann::json_schema::json_validator Validator; /** Stores the validator JSON object */

  nlohmann::json UserConfig; /** Stores the user config JSON object */

public:
  /**
   * @brief Construct a new Parallax Gen Config object
   *
   * @param ExePath Path to ParallaxGen.exe
   */
  ParallaxGenConfig(std::filesystem::path ExePath);

  /**
   * @brief Get the Config Validation object
   *
   * @return nlohmann::json config validation object
   */
  static auto getConfigValidation() -> nlohmann::json;

  /**
   * @brief Get the User Config File object
   *
   * @return std::filesystem::path Path to user config file
   */
  [[nodiscard]] auto getUserConfigFile() const -> std::filesystem::path;

  /**
   * @brief Loads the config files in the `cfg` folder
   */
  void loadConfig();

  /**
   * @brief Get the Mod Order object
   *
   * @return std::vector<std::wstring> Mod order
   */
  [[nodiscard]] auto getModOrder() -> std::vector<std::wstring>;

  /**
   * @brief Get mod priority for a given mod
   *
   * @param Mod mod to check
   * @return int priority of mod (-1 if not found)
   */
  [[nodiscard]] auto getModPriority(const std::wstring &Mod) -> int;

  /**
   * @brief Get the Mod Priority Map object
   *
   * @return std::unordered_map<std::wstring, int> Mod priority map
   */
  [[nodiscard]] auto getModPriorityMap() -> std::unordered_map<std::wstring, int>;

  /**
   * @brief Set the Mod Order object (also saves to user json)
   *
   * @param ModOrder new mod order to set
   */
  void setModOrder(const std::vector<std::wstring> &ModOrder);

  /**
   * @brief Get the current Params
   *
   * @return PGParams params
   */
  [[nodiscard]] auto getParams() const -> PGParams;

  /**
   * @brief Set params (also saves to user json)
   *
   * @param Params new params to set
   */
  void setParams(const PGParams &Params);

  /**
   * @brief Validates a given param struct
   *
   * @param Params Params to validate
   * @param Errors Error messages
   * @return true no validation errors
   * @return false validation errors
   */
  [[nodiscard]] static auto validateParams(const PGParams &Params, std::vector<std::string> &Errors) -> bool;

private:
  /**
   * @brief Parses JSON from a file
   *
   * @param JSONFile File to parse
   * @param Bytes Bytes to parse
   * @param J parsed JSON object
   * @return true no json errors
   * @return false unable to parse
   */
  static auto parseJSON(const std::filesystem::path &JSONFile, const std::vector<std::byte> &Bytes,
                        nlohmann::json &J) -> bool;

  /**
   * @brief Validates JSON file
   *
   * @param JSONFile File to validate
   * @param J JSON schema to validate against
   * @return true no validation errors
   * @return false validation errors
   */
  auto validateJSON(const std::filesystem::path &JSONFile, const nlohmann::json &J) -> bool;

  /**
   * @brief Adds a JSON config to the current config
   *
   * @param J JSON object to add
   */
  auto addConfigJSON(const nlohmann::json &J) -> void;

  /**
   * @brief Replaces any / with \\ in a JSON object
   *
   * @param JSON JSON object to change
   */
  static void replaceForwardSlashes(nlohmann::json &JSON);

  static auto utf16VectorToUTF8(const std::vector<std::wstring> &Vec) -> std::vector<std::string>;

  /**
   * @brief Saves user config to the user json file
   */
  void saveUserConfig();
};
