#pragma once

#include <unordered_map>

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
    std::unordered_map<NIFUtil::ShapeShader, PatcherShader::PatcherShaderObject> ShaderPatchers;
    std::unordered_map<NIFUtil::ShapeShader,
                       std::map<NIFUtil::ShapeShader, PatcherShaderTransform::PatcherShaderTransformObject>>
        ShaderTransformPatchers;
  };

  /**
   * @struct PatcherSet
   * @brief Stores the patcher factories for a given run
   */
  struct PatcherSet {
    std::unordered_map<NIFUtil::ShapeShader, PatcherShader::PatcherShaderFactory> ShaderPatchers;
    std::unordered_map<NIFUtil::ShapeShader,
                       std::map<NIFUtil::ShapeShader, PatcherShaderTransform::PatcherShaderTransformFactory>>
        ShaderTransformPatchers;
  };

  /**
   * @struct ShaderPatcherMatch
   * @brief Describes a match with transform properties
   */
  struct ShaderPatcherMatch {
    std::wstring Mod;
    NIFUtil::ShapeShader Shader;
    PatcherShader::PatcherMatch Match;
    NIFUtil::ShapeShader ShaderTransformTo;
  };

  /**
   * @brief Get the Winning Match object (checks mod priority)
   *
   * @param Matches Matches to check
   * @param NIFPath NIF path to check
   * @param ModPriority Mod priority map
   * @return ShaderPatcherMatch Winning match
   */
  static auto getWinningMatch(const std::vector<ShaderPatcherMatch> &Matches, const std::filesystem::path &NIFPath,
                              const std::unordered_map<std::wstring, int> *ModPriority = nullptr) -> ShaderPatcherMatch;

  /**
   * @brief Helper method to run a transform if needed on a match
   *
   * @param Match Match to run transform
   * @param Patchers Patcher set to use
   * @return ShaderPatcherMatch Transformed match
   */
  static auto applyTransformIfNeeded(const ShaderPatcherMatch &Match,
                                     const PatcherObjectSet &Patchers) -> ShaderPatcherMatch;
};
