#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

class ModManagerDirectory {

public:
  enum class ModManagerType {
    None,
    Vortex,
    ModOrganizer2
  };

private:
  std::unordered_map<std::filesystem::path, std::wstring> ModFileMap;

  std::unordered_set<std::wstring> AllMods;

  std::filesystem::path StagingDir;
  std::filesystem::path RequiredFile;
  ModManagerType MMType;

public:
  ModManagerDirectory(const ModManagerType &MMType);

  void populateInfo(const std::filesystem::path &RequiredFile, const std::filesystem::path &StagingDir = "");
  void populateModFileMap();

  [[nodiscard]] auto getModFileMap() const -> const std::unordered_map<std::filesystem::path, std::wstring> &;
  [[nodiscard]] auto getMod(const std::filesystem::path &RelPath) const -> std::wstring;

  // Helpers
  [[nodiscard]] static auto getModManagerTypes() -> std::vector<ModManagerType>;
  [[nodiscard]] static auto getStrFromModManagerType(const ModManagerType &Type) -> std::string;
  [[nodiscard]] static auto getModManagerTypeFromStr(const std::string &Type) -> ModManagerType;

private:
  void populateModFileMapMO2();
  void populateModFileMapVortex();
};
