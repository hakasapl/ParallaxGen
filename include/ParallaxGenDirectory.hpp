#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <vector>

#include "BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
  std::filesystem::path ExePath;

  std::vector<std::filesystem::path> HeightMaps;
  std::vector<std::filesystem::path> ComplexMaterialMaps;
  std::vector<std::filesystem::path> Meshes;
  std::vector<nlohmann::json> TruePBRConfigs;

  nlohmann::json PGConfig;

  static auto getPGConfigPath() -> std::wstring;

  static auto getTruePBRConfigFilenameFields() -> std::vector<std::string>;

  static auto getPGConfigValidation() -> nlohmann::json;

public:
  static auto getDefaultCubemapPath() -> std::filesystem::path;

  // constructor - calls the BethesdaDirectory constructor
  ParallaxGenDirectory(BethesdaGame BG, std::filesystem::path ExePath);

  // searches for height maps in the data directory
  void findHeightMaps();
  // searches for complex material maps in the data directory
  void findComplexMaterialMaps();
  // searches for Meshes in the data directory
  void findMeshes();
  // find truepbr config files
  void findTruePBRConfigs();

  // get the parallax gen config
  void loadPGConfig(const bool &LoadDefault = true);

  // add methods
  void addHeightMap(const std::filesystem::path &Path);

  void addComplexMaterialMap(const std::filesystem::path &Path);

  void addMesh(const std::filesystem::path &Path);

  // is methods
  [[nodiscard]] auto
  isHeightMap(const std::filesystem::path &Path) const -> bool;

  [[nodiscard]] auto
  isComplexMaterialMap(const std::filesystem::path &Path) const -> bool;

  [[nodiscard]] auto isMesh(const std::filesystem::path &Path) const -> bool;

  [[nodiscard]] auto defCubemapExists() -> bool;

  // get methods
  [[nodiscard]] auto
  getHeightMaps() const -> std::vector<std::filesystem::path>;

  [[nodiscard]] auto
  getComplexMaterialMaps() const -> std::vector<std::filesystem::path>;

  [[nodiscard]] auto getMeshes() const -> std::vector<std::filesystem::path>;

  [[nodiscard]] auto getTruePBRConfigs() const -> std::vector<nlohmann::json>;

  [[nodiscard]] auto
  getHeightMapFromBase(const std::string &Base) const -> std::string;

  [[nodiscard]] auto
  getComplexMaterialMapFromBase(const std::string &Base) const -> std::string;

  // Helpers
  static void mergeJSONSmart(nlohmann::json &Target,
                             const nlohmann::json &Source,
                             const nlohmann::json &Validation);

  static auto validateJSON(const nlohmann::json &Item,
                           const nlohmann::json &Validation,
                           const std::string &Key) -> bool;

  static auto jsonArrayToWString(const nlohmann::json &JSONArray)
      -> std::vector<std::wstring>;

  static void replaceForwardSlashes(nlohmann::json &JSON);

  static auto matchBase(const std::string &Base,
                        const std::vector<std::filesystem::path> &SearchList)
      -> std::filesystem::path;
};
