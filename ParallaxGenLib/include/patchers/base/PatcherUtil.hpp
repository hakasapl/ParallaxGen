#pragma once

#include <unordered_map>

#include "patchers/base/PatcherMeshGlobal.hpp"
#include "patchers/base/PatcherMeshPre.hpp"
#include "patchers/base/PatcherMeshShader.hpp"
#include "patchers/base/PatcherMeshShaderTransform.hpp"
#include "patchers/base/PatcherTextureGlobal.hpp"

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
    struct PatcherMeshObjectSet {
        std::vector<PatcherMeshGlobal::PatcherMeshGlobalObject> globalPatchers;
        std::vector<PatcherMeshPre::PatcherMeshPreObject> prePatchers;
        std::unordered_map<NIFUtil::ShapeShader, PatcherMeshShader::PatcherMeshShaderObject> shaderPatchers;
        std::unordered_map<NIFUtil::ShapeShader,
            std::map<NIFUtil::ShapeShader, PatcherMeshShaderTransform::PatcherMeshShaderTransformObject>>
            shaderTransformPatchers;
    };

    /**
     * @struct PatcherSet
     * @brief Stores the patcher factories for a given run
     */
    struct PatcherMeshSet {
        std::vector<PatcherMeshGlobal::PatcherMeshGlobalFactory> globalPatchers;
        std::vector<PatcherMeshPre::PatcherMeshPreFactory> prePatchers;
        std::unordered_map<NIFUtil::ShapeShader, PatcherMeshShader::PatcherMeshShaderFactory> shaderPatchers;
        std::unordered_map<NIFUtil::ShapeShader,
            std::map<NIFUtil::ShapeShader, PatcherMeshShaderTransform::PatcherMeshShaderTransformFactory>>
            shaderTransformPatchers;
    };

    struct PatcherTextureObjectSet {
        std::vector<PatcherTextureGlobal::PatcherGlobalObject> globalPatchers;
    };

    struct PatcherTextureSet {
        std::vector<PatcherTextureGlobal::PatcherGlobalFactory> globalPatchers;
    };

    /**
     * @struct ShaderPatcherMatch
     * @brief Describes a match with transform properties
     */
    struct ShaderPatcherMatch {
        std::wstring mod;
        NIFUtil::ShapeShader shader;
        PatcherMeshShader::PatcherMatch match;
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
    static auto applyTransformIfNeeded(const ShaderPatcherMatch& match, const PatcherMeshObjectSet& patchers)
        -> ShaderPatcherMatch;
};
