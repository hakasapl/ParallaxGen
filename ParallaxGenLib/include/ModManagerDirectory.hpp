#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class ModManagerDirectory {

public:
  enum class ModManagerType {
    None,
    ModOrganizer2,
    Vortex
  };

private:
  std::unordered_map<std::filesystem::path, std::wstring> ModFileMap;
  std::unordered_map<std::wstring, int> ModPriorityMap;

  std::filesystem::path StagingDir;
  std::filesystem::path RequiredFile;
  ModManagerType MMType;

public:
  ModManagerDirectory(const ModManagerType &MMType);

  void populateInfo(const std::vector<std::wstring> &ModOrder, const std::filesystem::path &RequiredFile, const std::filesystem::path &StagingDir = "");
  void populateModFileMap();

  [[nodiscard]] auto getModFileMap() const -> const std::unordered_map<std::filesystem::path, std::wstring> &;
  [[nodiscard]] auto getMod(const std::filesystem::path &RelPath) const -> std::wstring;
  [[nodiscard]] auto getModPriority(const std::wstring &Mod) const -> int;

private:
  void populateModFileMapMO2();
  void populateModFileMapVortex();
};
