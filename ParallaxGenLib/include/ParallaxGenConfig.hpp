#pragma once

#include <nlohmann/json.hpp>

#include "ParallaxGenDirectory.hpp"

// Forward declaration
class ParallaxGenDirectory;

class ParallaxGenConfig {
private:
  ParallaxGenDirectory *PGD;
  nlohmann::json PGConfig;
  std::filesystem::path ExePath;

  std::vector<std::vector<std::string>> CfgSuffixes;

public:
  ParallaxGenConfig(ParallaxGenDirectory *PGD, std::filesystem::path ExePath);
  static auto getConfigValidation() -> nlohmann::json;

  void loadConfig(const bool &LoadNative = true);
  [[nodiscard]] auto getConfig() const -> const nlohmann::json &;

  // Member variables
  [[nodiscard]] auto getSuffix(const unsigned int &Slot) const -> std::vector<std::string>;

private:
  auto validateConfig() -> bool;

  void addMissingFields(const nlohmann::json &Schema, nlohmann::json &Target);

  static void mergeJSONSmart(nlohmann::json &Target, const nlohmann::json &Source);

  static void replaceForwardSlashes(nlohmann::json &JSON);
};
