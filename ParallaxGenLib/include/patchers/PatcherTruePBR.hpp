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
        auto operator()(const std::tuple<std::wstring, std::wstring>& t) const -> std::size_t
        {
            std::size_t hash1 = std::hash<std::wstring> {}(std::get<0>(t));
            std::size_t hash2 = std::hash<std::wstring> {}(std::get<1>(t));

            // Combine the two hash values
            return hash1 ^ (hash2 << 1);
        }
    };

    // Options
    static bool s_checkPaths;

public:
    /**
     * @brief Get the True PBR Configs
     *
     * @return std::map<size_t, nlohmann::json>& JSON objects
     */
    static auto getTruePBRConfigs() -> std::map<size_t, nlohmann::json>&;

    /**
     * @brief Get the Path Lookup JSONs objects
     *
     * @return std::map<size_t, nlohmann::json>& JSON objects
     */
    static auto getPathLookupJSONs() -> std::map<size_t, nlohmann::json>&;

    /**
     * @brief Get the Path Lookup Cache object
     *
     * @return std::unordered_map<std::tuple<std::wstring, std::wstring>, bool, TupleStrHash>& Cache results for path
     * lookups
     */
    static auto getPathLookupCache() -> std::unordered_map<std::tuple<std::wstring, std::wstring>, bool, TupleStrHash>&;

    /**
     * @brief Get the True PBR Diffuse Inverse lookup table
     *
     * @return std::map<std::wstring, std::vector<size_t>>& Lookup
     */
    static auto getTruePBRDiffuseInverse() -> std::map<std::wstring, std::vector<size_t>>&;

    /**
     * @brief Get the True PBR Normal Inverse lookup table
     *
     * @return std::map<std::wstring, std::vector<size_t>>& Lookup
     */
    static auto getTruePBRNormalInverse() -> std::map<std::wstring, std::vector<size_t>>&;

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
     * @param pbrJSONs PBR jsons files to load as PBR configs
     */
    static void loadStatics(const std::vector<std::filesystem::path>& pbrJSONs);

    /**
     * @brief Get the Shader Type object (TruePBR)
     *
     * @return NIFUtil::ShapeShader Shader type (TruePBR)
     */
    static auto getShaderType() -> NIFUtil::ShapeShader;

    /**
     * @brief Construct a new Patcher True PBR patcher
     *
     * @param nifPath NIF Path to be patched
     * @param nif NIF object to be patched
     */
    PatcherTruePBR(std::filesystem::path nifPath, nifly::NifFile* nif);

    /**
     * @brief Check if shape can accomodate truepbr (without slots)
     *
     * @param nifShape Shape to check
     * @return true Can accomodate
     * @return false Cannot accomodate
     */
    auto canApply(nifly::NiShape& nifShape) -> bool override;

    /**
     * @brief Check if shape can accomodate truepbr (with slots)
     *
     * @param nifShape Shape to check
     * @param[out] matches Matches found
     * @return true Found matches
     * @return false Didn't find matches
     */
    auto shouldApply(nifly::NiShape& nifShape, std::vector<PatcherMatch>& matches) -> bool override;

    /**
     * @brief Check if slots can accomodate truepbr
     *
     * @param oldSlots Slots to check
     * @param[out] matches Matches found
     * @return true Found matches
     * @return false Didn't find matches
     */
    auto shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS>& oldSlots,
        std::vector<PatcherMatch>& matches) -> bool override;

    /**
     * @brief Applies a match to a shape
     *
     * @param nifShape Shape to patch
     * @param match Match to apply
     * @param[out] nifModified Whether NIF was modified or not
     * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots of shape
     */
    auto applyPatch(nifly::NiShape& nifShape, const PatcherMatch& match,
        bool& nifModified) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

    /**
     * @brief Apply a match to slots
     *
     * @param oldSlots Slots to patch
     * @param[out] match Match to apply
     * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots
     */
    auto applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS>& oldSlots,
        const PatcherMatch& match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> override;

    /**
     * @brief Apply pbr shader to a shape
     *
     * @param nifShape Shape to apply shader to
     * @param nifModified Whether the NIF was modified
     */
    void applyShader(nifly::NiShape& nifShape, bool& nifModified) override;

    void processNewTXSTRecord(const PatcherMatch& match, const std::string& edid = {}) override;

    /**
     * @brief Load PBR options string
     *
     * @param optionsStr string to load
     */
    static void loadOptions(std::unordered_set<std::string>& optionsStr);

private:
    /**
     * @brief Applies a single JSON config to a shape
     *
     * @param nifShape Shape to patch
     * @param truePBRData Data to patch
     * @param matchedPath Matched path (PBR prefix)
     * @param[out] nifModified Whether NIF was modified
     * @param[out] newSlots New slots of shape
     */
    void applyOnePatch(nifly::NiShape* nifShape, nlohmann::json& truePBRData, const std::wstring& matchedPath,
        bool& nifModified, std::array<std::wstring, NUM_TEXTURE_SLOTS>& newSlots);

    static void applyOnePatchSwapJSON(const nlohmann::json& truePBRData, nlohmann::json& output);

    /**
     * @brief Applies a single JSON config to slots
     *
     * @param oldSlots Slots to patch
     * @param truePBRData Data to patch
     * @param matchedPath Matched path (PBR prefix)
     * @return std::array<std::wstring, NUM_TEXTURE_SLOTS> New slots after patch
     */
    static void applyOnePatchSlots(std::array<std::wstring, NUM_TEXTURE_SLOTS>& slots,
        const nlohmann::json& truePBRData, const std::wstring& matchedPath);

    /**
     * @brief Enables truepbr on a shape (pbr: true in JSON)
     *
     * @param nifShape Shape to enable truepbr on
     * @param nifShader Shader of shape
     * @param nifShaderBSLSP Properties of shader
     * @param truePBRData Data to enable truepbr with
     * @param matchedPath Matched path (PBR prefix)
     * @param[out] nifModified Whether NIF was modified
     * @param[out] newSlots New slots of shape
     */
    void enableTruePBROnShape(nifly::NiShape* nifShape, nifly::NiShader* nifShader,
        nifly::BSLightingShaderProperty* nifShaderBSLSP, nlohmann::json& truePBRData, const std::wstring& matchedPath,
        bool& nifModified, std::array<std::wstring, NUM_TEXTURE_SLOTS>& newSlots);

    // TruePBR Helpers

    /**
     * @brief Calculate ABS of 2-element vector
     *
     * @param v vector to calculate abs of
     * @return nifly::Vector2 ABS of vector
     */
    static auto abs2(nifly::Vector2 v) -> nifly::Vector2;

    /**
     * @brief Math that calculates auto UV scale for a shape
     *
     * @param uvs UVs of shape
     * @param verts Vertices of shape
     * @param tris Triangles of shape
     * @return nifly::Vector2
     */
    static auto autoUVScale(const std::vector<nifly::Vector2>* uvs, const std::vector<nifly::Vector3>* verts,
        std::vector<nifly::Triangle>& tris) -> nifly::Vector2;

    /**
     * @brief Checks if a JSON field has a key and that key is true
     *
     * @param json JSON to check
     * @param key Key to check
     * @return true has key and is true
     * @return false does not have key or is not true
     */
    static auto flag(const nlohmann::json& json, const char* key) -> bool;

    /**
     * @brief Get the Slot Match for a given lookup (diffuse or normal)
     *
     * @param[out] truePBRData Data that matched
     * @param texName Texture name to match
     * @param lookup Lookup table to use
     * @param slotLabel Slot label to use
     * @param nifPath NIF path to use
     */
    static void getSlotMatch(std::map<size_t, std::tuple<nlohmann::json, std::wstring>>& truePBRData,
        const std::wstring& texName, const std::map<std::wstring, std::vector<size_t>>& lookup,
        const std::wstring& slotLabel, const std::wstring& nifPath);

    /**
     * @brief Get path contains match for diffuse
     *
     * @param[out] truePBRData Data that matched
     * @param[out] diffuse Texture name to patch
     * @param nifPath NIF path to use
     */
    static void getPathContainsMatch(std::map<size_t, std::tuple<nlohmann::json, std::wstring>>& truePBRData,
        const std::wstring& diffuse, const std::wstring& nifPath);

    /**
     * @brief Inserts truepbr data if criteria is met
     *
     * @param[out] truePBRData Data to update
     * @param texName Texture name to insert
     * @param cfg Config ID
     * @param nifPath NIF path to use
     */
    static void insertTruePBRData(std::map<size_t, std::tuple<nlohmann::json, std::wstring>>& truePBRData,
        const std::wstring& texName, size_t cfg, const std::wstring& nifPath);
};
