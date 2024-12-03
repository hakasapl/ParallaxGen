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
  std::vector<std::wstring> InferredOrder;

  ModManagerType MMType;

public:
  ModManagerDirectory(const ModManagerType &MMType);

  [[nodiscard]] auto getModFileMap() const -> const std::unordered_map<std::filesystem::path, std::wstring> &;
  [[nodiscard]] auto getMod(const std::filesystem::path &RelPath) const -> std::wstring;
  [[nodiscard]] auto getInferredOrder() const -> const std::vector<std::wstring> &;

  static auto getMO2ProfilesFromInstanceDir(const std::filesystem::path &InstanceDir) -> std::vector<std::wstring>;

  void populateModFileMapMO2(const std::filesystem::path &InstanceDir, const std::wstring &Profile);
  void populateModFileMapVortex(const std::filesystem::path &DeploymentDir);

  // Helpers
  [[nodiscard]] static auto getModManagerTypes() -> std::vector<ModManagerType>;
  [[nodiscard]] static auto getStrFromModManagerType(const ModManagerType &Type) -> std::string;
  [[nodiscard]] static auto getModManagerTypeFromStr(const std::string &Type) -> ModManagerType;
};
