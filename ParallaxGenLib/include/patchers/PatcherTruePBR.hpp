#pragma once

#include <Geometry.hpp>
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

/**
 * @class PatcherTruePBR
 * @brief Patcher for True PBR
 */
class PatcherTruePBR : public PatcherShader {
private:
  // Static caches

  /**
   * @struct TupleStrHash
   * @brief Key hash for storing a tuple of two strings
   */
  struct TupleStrHash {
    auto operator()(const std::tuple<std::wstring, std::wstring> &T) const -> std::size_t {
      std::size_t Hash1 = std::hash<std::wstring>{}(std::get<0>(T));
      std::size_t Hash2 = std::hash<std::wstring>{}(std::get<1>(T));

      // Combine the two hash values
      return Hash1 ^ (Hash2 << 1);
    }
  };

  std::unordered_map<uint32_t, std::vector<PatcherMatch>> MatchedTextureSets;  /** Set that stores already matched texture sets */

public:
  /**
   * @brief Get the True PBR Configs
   *
   * @return std::map<size_t, nlohmann::json>& JSON objects
   */
  static auto getTruePBRConfigs() -> std::map<size_t, nlohmann::json> &;

  /**
   * @brief Get the Path Lookup JSONs objects
   *
   * @return std::map<size_t, nlohmann::json>& JSON objects
   */
  static auto getPathLookupJSONs() -> std::map<size_t, nlohmann::json> &;

  /**
   * @brief Get the Path Lookup Cache object
   *
   * @return std::unordered_map<std::tuple<std::wstring, std::wstring>, bool, TupleStrHash>& Cache results for path lookups
   */
  static auto getPathLookupCache() -> std::unordered_map<std::tuple<std::wstring, std::wstring>, bool, TupleStrHash> &;

  /**
   * @brief Get the True PBR Diffuse Inverse lookup table
   *
   * @return std::map<std::wstring, std::vector<size_t>>& Lookup
   */
  static auto getTruePBRDiffuseInverse() -> std::map<std::wstring, std::vector<size_t>> &;

  /**
   * @brief Get the True PBR Normal Inverse lookup table
   *
   * @return std::map<std::wstring, std::vector<size_t>>& Lookup
   */
  static auto getTruePBRNormalInverse() -> std::map<std::wstring, std::vector<size_t>> &;

  /**
   * @brief Get the True PBR Config Filename Fields (fields that have paths)
   *
   * @return std::vector<std::string> Filename fields
   */
  static auto getTruePBRConfigFilenameFields() -> std::vector<std::string>;

  /**
   * @brief Get the Factory object for this patcher
   *
   * @return PatcherShader::PatcherShaderFactory Factory object
   */
  static auto getFactory() -> PatcherShader::PatcherShaderFactory;

  /**
   * @brief Load statics from a list of PBRJSONs
   *
   * @param PBRJSONs PBR jsons files to load as PBR configs
   */
  static void loadStatics(const std::vector<std::filesystem::path> &PBRJSONs);

  /**
   * @brief Get the Shader Type object (TruePBR)
   *
   * @return NIFUtil::ShapeShader Shader type (TruePBR)
   */
  static auto getShaderType() -> NIFUtil::ShapeShader;

  /**
   * @brief Construct a new Patcher True PBR patcher
   *
   * @param NIFPath NIF Path to be patched
   * @param NIF NIF object to be patched
   */
  PatcherTruePBR(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  /**
   * @brief Check if shape can accomodate truepbr (without slots)
   *
   * @param NIFShape Shape to check
   * @return true Can accomodate
   * @return false Cannot accomodate
   */
  auto canApply(nifly::NiShape &NIFShape) -> bool override;

  /**
   * @brief Check if shape can accomodate truepbr (with slots)
   *
   * @param NIFShape Shape to check
   * @param[out] Matches Matches found
   * @return true Found matches
   * @return false Didn't find matches
   */
  auto shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool override;

  /**
   * @brief Check if slots can accomodate truepbr
   *
   * @param OldSlots Slots to check
   * @param[out] Matches Matches found
   * @return true Found matches
   * @return false Didn't find matches
   */
  auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                   std::vector<PatcherMatch> &Matches) -> bool override;

  /**
   * @brief Applies a match to a shape
   *
   * @param NIFShape Shape to patch
   * @param Match Match to apply
   * @param[out] NIFModified Whether NIF was modified or not
   * @param[out] ShapeDeleted Whether shape was deleted or not
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots of shape
   */
  auto applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                  bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  /**
   * @brief Apply a match to slots
   *
   * @param OldSlots Slots to patch
   * @param[out] Match Match to apply
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots
   */
  auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                       const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

  /**
   * @brief Apply neutral texture to slots
   *
   * @param Slots Slots to apply neutral tex to
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots
   */
  auto applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
      -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

private:
  /**
   * @brief Applies a single JSON config to a shape
   *
   * @param NIFShape Shape to patch
   * @param TruePBRData Data to patch
   * @param MatchedPath Matched path (PBR prefix)
   * @param[out] NIFModified Whether NIF was modified
   * @param[out] ShapeDeleted Whether shape was deleted
   * @param[out] NewSlots New slots of shape
   */
  void applyOnePatch(nifly::NiShape *NIFShape, nlohmann::json &TruePBRData, const std::wstring &MatchedPath,
                     bool &NIFModified, bool &ShapeDeleted,
                     std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) const;

  /**
   * @brief Applies a single JSON config to slots
   *
   * @param OldSlots Slots to patch
   * @param TruePBRData Data to patch
   * @param MatchedPath Matched path (PBR prefix)
   * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots after patch
   */
  static void applyOnePatchSlots(std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots,
                                 const nlohmann::json &TruePBRData,
                                 const std::wstring &MatchedPath);

  /**
   * @brief Enables truepbr on a shape (pbr: true in JSON)
   *
   * @param NIFShape Shape to enable truepbr on
   * @param NIFShader Shader of shape
   * @param NIFShaderBSLSP Properties of shader
   * @param TruePBRData Data to enable truepbr with
   * @param MatchedPath Matched path (PBR prefix)
   * @param[out] NIFModified Whether NIF was modified
   * @param[out] NewSlots New slots of shape
   */
  void enableTruePBROnShape(nifly::NiShape *NIFShape, nifly::NiShader *NIFShader,
                            nifly::BSLightingShaderProperty *NIFShaderBSLSP, nlohmann::json &TruePBRData,
                            const std::wstring &MatchedPath, bool &NIFModified,
                            std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) const;

  // TruePBR Helpers

  /**
   * @brief Calculate ABS of 2-element vector
   *
   * @param V vector to calculate abs of
   * @return nifly::Vector2 ABS of vector
   */
  static auto abs2(nifly::Vector2 V) -> nifly::Vector2;

  /**
   * @brief Math that calculates auto UV scale for a shape
   *
   * @param UVS UVs of shape
   * @param Verts Vertices of shape
   * @param Tris Triangles of shape
   * @return nifly::Vector2
   */
  static auto autoUVScale(const std::vector<nifly::Vector2> *UVS, const std::vector<nifly::Vector3> *Verts,
                          std::vector<nifly::Triangle> &Tris) -> nifly::Vector2;

  /**
   * @brief Checks if a JSON field has a key and that key is true
   *
   * @param JSON JSON to check
   * @param Key Key to check
   * @return true has key and is true
   * @return false does not have key or is not true
   */
  static auto flag(const nlohmann::json &JSON, const char *Key) -> bool;

  /**
   * @brief Get the Slot Match for a given lookup (diffuse or normal)
   *
   * @param[out] TruePBRData Data that matched
   * @param TexName Texture name to match
   * @param Lookup Lookup table to use
   * @param SlotLabel Slot label to use
   * @param NIFPath NIF path to use
   */
  static auto getSlotMatch(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                           const std::wstring &TexName, const std::map<std::wstring, std::vector<size_t>> &Lookup,
                           const std::wstring &SlotLabel, const std::wstring &NIFPath, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots) -> void;

  /**
   * @brief Get path contains match for diffuse
   *
   * @param[out] TruePBRData Data that matched
   * @param[out] Diffuse Texture name to patch
   * @param NIFPath NIF path to use
   */
  static auto getPathContainsMatch(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                   const std::wstring &Diffuse, const std::wstring &NIFPath, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots) -> void;

  /**
   * @brief Inserts truepbr data if criteria is met
   *
   * @param[out] TruePBRData Data to update
   * @param TexName Texture name to insert
   * @param Cfg Config ID
   * @param NIFPath NIF path to use
   */
  static auto insertTruePBRData(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                const std::wstring &TexName, size_t Cfg, const std::wstring &NIFPath, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots) -> void;
};
