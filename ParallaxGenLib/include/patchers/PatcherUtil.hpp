#pragma once

#include <unordered_map>

#include "patchers/PatcherShader.hpp"
#include "patchers/PatcherShaderTransform.hpp"

#include "NIFUtil.hpp"

class PatcherUtil {
public:
  struct PatcherObjectSet {
    std::unordered_map<NIFUtil::ShapeShader, std::unique_ptr<PatcherShader>> ShaderPatchers;
    std::unordered_map<NIFUtil::ShapeShader, std::map<NIFUtil::ShapeShader, std::unique_ptr<PatcherShaderTransform>>>
        ShaderTransformPatchers;
  };

  struct PatcherSet {
    std::unordered_map<NIFUtil::ShapeShader,
                       std::function<std::unique_ptr<PatcherShader>(std::filesystem::path, nifly::NifFile *)>>
        ShaderPatchers;
    std::unordered_map<NIFUtil::ShapeShader,
                       std::map<NIFUtil::ShapeShader, std::function<std::unique_ptr<PatcherShaderTransform>(
                                                          std::filesystem::path, nifly::NifFile *)>>>
        ShaderTransformPatchers;
  };

  struct ShaderPatcherMatch {
    std::wstring Mod;
    NIFUtil::ShapeShader Shader;
    PatcherShader::PatcherMatch Match;
    NIFUtil::ShapeShader ShaderTransformTo;
    const std::unique_ptr<PatcherShaderTransform> *Transform;
  };
};
