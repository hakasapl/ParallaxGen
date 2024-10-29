#pragma once

#include <mutex>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

#include "NIFUtil.hpp"

// Forward declaration
class ParallaxGenDirectory;

class ParallaxGenConfig {
private:
  std::filesystem::path ExePath;

  // Config Structures
  std::unordered_set<std::wstring> NIFBlocklist {};
  std::unordered_set<std::wstring> DynCubemapBlocklist{};
  std::unordered_map<std::filesystem::path, NIFUtil::TextureType> ManualTextureMaps{};
  std::unordered_set<std::wstring> VanillaBSAList{};

  std::mutex ModOrderMutex;
  std::vector<std::wstring> ModOrder;
  std::unordered_map<std::wstring, int> ModPriority;

  // Validator
  nlohmann::json_schema::json_validator Validator;

public:
  ParallaxGenConfig(std::filesystem::path ExePath);
  static auto getConfigValidation() -> nlohmann::json;
  auto getUserConfigFile() const -> std::filesystem::path;

  void loadConfig(const bool &LoadNative = true);

  [[nodiscard]] auto getNIFBlocklist() const -> const std::unordered_set<std::wstring> &;

  [[nodiscard]] auto getDynCubemapBlocklist() const -> const std::unordered_set<std::wstring> &;

  [[nodiscard]] auto getManualTextureMaps() const -> const std::unordered_map<std::filesystem::path, NIFUtil::TextureType> &;

  [[nodiscard]] auto getVanillaBSAList() const -> const std::unordered_set<std::wstring> &;

  [[nodiscard]] auto getModOrder() -> std::vector<std::wstring>;
  [[nodiscard]] auto getModPriority(const std::wstring &Mod) -> int;
  void setModOrder(const std::vector<std::wstring> &ModOrder);

private:
  static auto parseJSON(const std::filesystem::path &JSONFile, const std::vector<std::byte> &Bytes, nlohmann::json &J) -> bool;

  auto validateJSON(const std::filesystem::path &JSONFile, const nlohmann::json &J) -> bool;

  auto addConfigJSON(const nlohmann::json &J) -> void;

  static void replaceForwardSlashes(nlohmann::json &JSON);

  void saveUserConfig() const;
};
