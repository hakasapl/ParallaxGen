#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "NIFUtil.hpp"
#include "patchers/PatcherShader.hpp"

constexpr unsigned TEXTURE_STR_LENGTH = 9;

class PatcherTruePBR : public PatcherShader {
private:
  // Static caches
  struct TupleStrHash {
    auto operator()(const std::tuple<std::wstring, std::wstring> &T) const -> std::size_t {
      std::size_t Hash1 = std::hash<std::wstring>{}(std::get<0>(T));
      std::size_t Hash2 = std::hash<std::wstring>{}(std::get<1>(T));

      // Combine the two hash values
      return Hash1 ^ (Hash2 << 1);
    }
  };

  // Set that stores already matched texture sets
  std::unordered_map<uint32_t, std::vector<PatcherMatch>> MatchedTextureSets;

public:
  static auto getTruePBRConfigs() -> std::map<size_t, nlohmann::json> &;
  static auto getPathLookupJSONs() -> std::map<size_t, nlohmann::json> &;
  static auto getPathLookupCache() -> std::unordered_map<std::tuple<std::wstring, std::wstring>, bool, TupleStrHash> &;
  static auto getTruePBRDiffuseInverse() -> std::map<std::wstring, std::vector<size_t>> &;
  static auto getTruePBRNormalInverse() -> std::map<std::wstring, std::vector<size_t>> &;
  static auto getTruePBRConfigFilenameFields() -> std::vector<std::string>;

  static auto getFactory() -> PatcherShader::PatcherShaderFactory;
  static void loadStatics(const std::vector<std::filesystem::path> &PBRJSONs);
  static auto getShaderType() -> NIFUtil::ShapeShader;

  PatcherTruePBR(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  // check if truepbr should be enabled on shape
  auto canApply(nifly::NiShape &NIFShape) -> bool override;
  auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool override;
  auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                   std::vector<PatcherMatch> &Matches) -> bool override;

  // applies truepbr config on a shape in a NIF (always applies with config, but
  // maybe PBR is disabled)
  auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                  bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;
  auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                       const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  auto applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
      -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

private:
  void applyOnePatch(nifly::NiShape *NIFShape, nlohmann::json &TruePBRData, const std::wstring &MatchedPath,
                     bool &NIFModified, bool &ShapeDeleted,
                     std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) const;

  static auto applyOnePatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                 const nlohmann::json &TruePBRData,
                                 const std::wstring &MatchedPath) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

  // enables truepbr on a shape in a NIF (If PBR is enabled)
  void enableTruePBROnShape(nifly::NiShape *NIFShape, nifly::NiShader *NIFShader,
                            nifly::BSLightingShaderProperty *NIFShaderBSLSP, nlohmann::json &TruePBRData,
                            const std::wstring &MatchedPath, bool &NIFModified,
                            std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) const;

  // TruePBR Helpers
  // Calculates 2d absolute value of a vector2
  static auto abs2(nifly::Vector2 V) -> nifly::Vector2;

  // Auto UV scale magic that I don't understand
  static auto autoUVScale(const std::vector<nifly::Vector2> *UVS, const std::vector<nifly::Vector3> *Verts,
                          std::vector<nifly::Triangle> &Tris) -> nifly::Vector2;

  // Checks if a json object has a key
  static auto flag(const nlohmann::json &JSON, const char *Key) -> bool;

  static auto getSlotMatch(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                           const std::wstring &TexName, const std::map<std::wstring, std::vector<size_t>> &Lookup,
                           const std::wstring &SlotLabel, const std::wstring &NIFPath) -> void;

  static auto getPathContainsMatch(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                   const std::wstring &Diffuse, const std::wstring &NIFPath) -> void;

  static auto insertTruePBRData(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                const std::wstring &TexName, size_t Cfg, const std::wstring &NIFPath) -> void;
};
