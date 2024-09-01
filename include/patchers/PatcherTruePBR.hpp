#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>

#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"

#define TEXTURE_STR_LENGTH 9

class PatcherTruePBR {
private:
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;

  // Static caches
  struct TupleStrHash {
    auto operator()(const std::tuple<std::string, std::string> &T) const -> std::size_t {
      std::size_t Hash1 = std::hash<std::string>{}(std::get<0>(T));
      std::size_t Hash2 = std::hash<std::string>{}(std::get<1>(T));

      // Combine the two hash values
      return Hash1 ^ (Hash2 << 1);
    }
  };

public:
  static auto getTruePBRConfigs() -> std::map<size_t, nlohmann::json> &;
  static auto getPathLookupJSONs() -> std::map<size_t, nlohmann::json> &;
  static auto getPathLookupCache() -> std::unordered_map<std::tuple<std::string, std::string>, bool, TupleStrHash> &;
  static auto getTruePBRDiffuseInverse() -> std::map<std::string, std::vector<size_t>> &;
  static auto getTruePBRNormalInverse() -> std::map<std::string, std::vector<size_t>> &;

  PatcherTruePBR(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  static void loadPatcherBuffers(const std::map<size_t, nlohmann::json> &PBRJSONs);

  // check if truepbr should be enabled on shape
  auto shouldApply(const std::array<std::string, NUM_TEXTURE_SLOTS> &SearchPrefixes, bool &EnableResult,
                   std::map<size_t, std::tuple<nlohmann::json, std::string>> &TruePBRData) -> ParallaxGenTask::PGResult;

  // applies truepbr config on a shape in a NIF (always applies with config, but
  // maybe PBR is disabled)
  auto applyPatch(nifly::NiShape *NIFShape, nlohmann::json &TruePBRData, const std::string &MatchedPath,
                  bool &NIFModified) const -> ParallaxGenTask::PGResult;

private:
  // enables truepbr on a shape in a NIF (If PBR is enabled)
  auto enableTruePBROnShape(nifly::NiShape *NIFShape, nifly::NiShader *NIFShader,
                            nifly::BSLightingShaderProperty *NIFShaderBSLSP, nlohmann::json &TruePBRData,
                            const std::string &MatchedPath, bool &NIFModified) const -> ParallaxGenTask::PGResult;

  // TruePBR Helpers
  // Calculates 2d absolute value of a vector2
  static auto abs2(nifly::Vector2 V) -> nifly::Vector2;

  // Auto UV scale magic that I don't understand
  static auto autoUVScale(const std::vector<nifly::Vector2> *UVS, const std::vector<nifly::Vector3> *Verts,
                          std::vector<nifly::Triangle> &Tris) -> nifly::Vector2;

  // Checks if a json object has a key
  static auto flag(nlohmann::json &JSON, const char *Key) -> bool;

  auto getSlotMatch(std::map<size_t, std::tuple<nlohmann::json, std::string>> &TruePBRData,
                    std::string &PriorityJSONFile, const std::string &TexName,
                    const std::map<std::string, std::vector<size_t>> &Lookup) -> void;

  auto getPathContainsMatch(std::map<size_t, std::tuple<nlohmann::json, std::string>> &TruePBRData,
                            std::string &PriorityJSONFile, const std::string &Diffuse) -> void;

  auto insertTruePBRData(std::map<size_t, std::tuple<nlohmann::json, std::string>> &TruePBRData,
                         std::string &PriorityJSONFile, const std::string &TexName, size_t Cfg) -> void;
};
