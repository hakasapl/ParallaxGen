#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

class ModManagerDirectory {

public:
  enum class ModManagerType {
    None,
    ModOrganizer2,
    Vortex
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

private:
  void populateModFileMapMO2();
  void populateModFileMapVortex();
};
