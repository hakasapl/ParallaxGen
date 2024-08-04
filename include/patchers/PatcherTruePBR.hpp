#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <vector>

#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"

class PatcherTruePBR {
private:
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;

public:
  PatcherTruePBR(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  // check if truepbr should be enabled on shape
  auto
  shouldApply(nifly::NiShape *NIFShape, const std::vector<nlohmann::json> &TPBRConfigs,
              const std::array<std::string, NUM_TEXTURE_SLOTS> &SearchPrefixes, bool &EnableResult,
              std::vector<std::tuple<nlohmann::json, std::string>> &TruePBRData) const -> ParallaxGenTask::PGResult;

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
};
