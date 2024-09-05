#pragma once

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>

#include "BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
  std::vector<std::filesystem::path> HeightMaps;
  std::map<std::string, std::filesystem::path> HeightMapsBases;
  std::vector<std::filesystem::path> ComplexMaterialMaps;
  std::map<std::string, std::filesystem::path> ComplexMaterialMapsBases;
  std::vector<std::filesystem::path> Meshes;
  std::vector<std::filesystem::path> PGConfigs;

  std::map<size_t, nlohmann::json> TruePBRConfigs;

  static auto getTruePBRConfigFilenameFields() -> std::vector<std::string>;
  static auto getConfigPath() -> std::wstring;

public:
  static auto getDefaultCubemapPath() -> std::filesystem::path;

  // constructor - calls the BethesdaDirectory constructor
  ParallaxGenDirectory(BethesdaGame BG);

  // searches for height maps in the data directory
  void findHeightMaps(const std::vector<std::wstring> &Allowlist, const std::vector<std::wstring> &Blocklist,
                      const std::vector<std::wstring> &ArchiveBlocklist);
  // searches for complex material maps in the data directory
  void findComplexMaterialMaps(const std::vector<std::wstring> &Allowlist, const std::vector<std::wstring> &Blocklist,
                               const std::vector<std::wstring> &ArchiveBlocklist);
  // searches for Meshes in the data directory
  void findMeshes(const std::vector<std::wstring> &Allowlist, const std::vector<std::wstring> &Blocklist,
                  const std::vector<std::wstring> &ArchiveBlocklist);
  // find truepbr config files
  void findTruePBRConfigs(const std::vector<std::wstring> &Allowlist, const std::vector<std::wstring> &Blocklist,
                          const std::vector<std::wstring> &ArchiveBlocklist);
  // get the parallax gen config
  void findPGConfigs();

  void buildBaseMaps(const std::vector<std::vector<std::string>> &Suffixes);

  // add methods
  void addHeightMap(const std::filesystem::path &Path, const std::string &Base);

  void addComplexMaterialMap(const std::filesystem::path &Path, const std::string &Base);

  void addMesh(const std::filesystem::path &Path);

  void setHeightMaps(const std::vector<std::filesystem::path> &Paths);

  void setComplexMaterialMaps(const std::vector<std::filesystem::path> &Paths);

  void setMeshes(const std::vector<std::filesystem::path> &Paths);

  // is methods
  [[nodiscard]] auto isHeightMap(const std::filesystem::path &Path) const -> bool;

  [[nodiscard]] auto isComplexMaterialMap(const std::filesystem::path &Path) const -> bool;

  [[nodiscard]] auto isMesh(const std::filesystem::path &Path) const -> bool;

  [[nodiscard]] auto defCubemapExists() -> bool;

  // get methods
  [[nodiscard]] auto getHeightMaps() const -> const std::vector<std::filesystem::path> &;
  [[nodiscard]] auto getHeightMapsBases() const -> const std::map<std::string, std::filesystem::path> &;

  [[nodiscard]] auto getComplexMaterialMaps() const -> const std::vector<std::filesystem::path> &;
  [[nodiscard]] auto getComplexMaterialMapsBases() const -> const std::map<std::string, std::filesystem::path> &;

  [[nodiscard]] auto getMeshes() const -> const std::vector<std::filesystem::path> &;

  [[nodiscard]] auto getTruePBRConfigs() const -> const std::map<size_t, nlohmann::json> &;

  [[nodiscard]] auto getPGConfigs() const -> const std::vector<std::filesystem::path> &;

private:
  static void buildBaseMap(std::map<std::string, std::filesystem::path> &BaseMap,
                           std::vector<std::filesystem::path> &Files, const std::vector<std::string> &Suffixes);
};
