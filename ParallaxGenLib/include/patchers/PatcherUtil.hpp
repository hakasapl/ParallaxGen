#pragma once

#include <unordered_map>

#include "patchers/PatcherGlobal.hpp"
#include "patchers/PatcherShader.hpp"
#include "patchers/PatcherShaderTransform.hpp"

#include "NIFUtil.hpp"

/**
 * @class PatcherUtil
 * @brief Utility class for code that uses patchers
 */
class PatcherUtil {
public:
    /**
     * @struct PatcherObjectSet
     * @brief Stores the patcher objects for a given run
     */
    struct PatcherObjectSet {
        std::vector<PatcherGlobal::PatcherGlobalObject> globalPatchers;
        std::unordered_map<NIFUtil::ShapeShader, PatcherShader::PatcherShaderObject> shaderPatchers;
        std::unordered_map<NIFUtil::ShapeShader,
            std::map<NIFUtil::ShapeShader, PatcherShaderTransform::PatcherShaderTransformObject>>
            shaderTransformPatchers;
    };

    /**
     * @struct PatcherSet
     * @brief Stores the patcher factories for a given run
     */
    struct PatcherSet {
        std::vector<PatcherGlobal::PatcherGlobalFactory> globalPatchers;
        std::unordered_map<NIFUtil::ShapeShader, PatcherShader::PatcherShaderFactory> shaderPatchers;
        std::unordered_map<NIFUtil::ShapeShader,
            std::map<NIFUtil::ShapeShader, PatcherShaderTransform::PatcherShaderTransformFactory>>
            shaderTransformPatchers;
    };

    /**
     * @struct ShaderPatcherMatch
     * @brief Describes a match with transform properties
     */
    struct ShaderPatcherMatch {
        std::wstring mod;
        NIFUtil::ShapeShader shader;
        PatcherShader::PatcherMatch match;
        NIFUtil::ShapeShader shaderTransformTo;
    };

    struct ConflictModResults {
        std::unordered_map<std::wstring, std::tuple<std::set<NIFUtil::ShapeShader>, std::unordered_set<std::wstring>>>
            mods;
        std::mutex mutex;
    };

    /**
     * @brief Get the Winning Match object (checks mod priority)
     *
     * @param Matches Matches to check
     * @param NIFPath NIF path to check
     * @param ModPriority Mod priority map
     * @return ShaderPatcherMatch Winning match
     */
    static auto getWinningMatch(const std::vector<ShaderPatcherMatch>& matches,
        const std::unordered_map<std::wstring, int>* modPriority = nullptr) -> ShaderPatcherMatch;

    /**
     * @brief Helper method to run a transform if needed on a match
     *
     * @param Match Match to run transform
     * @param Patchers Patcher set to use
     * @return ShaderPatcherMatch Transformed match
     */
    static auto applyTransformIfNeeded(
        const ShaderPatcherMatch& match, const PatcherObjectSet& patchers) -> ShaderPatcherMatch;
};
