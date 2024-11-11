#pragma once

#include "NIFUtil.hpp"
#include "patchers/PatcherShaderTransform.hpp"

class PatcherUpgradeParallaxToCM : public PatcherShaderTransform
{
public:
  static auto getFactory() -> PatcherShaderTransform::PatcherShaderTransformFactory;
  static auto getFromShader() -> NIFUtil::ShapeShader;
  static auto getToShader() -> NIFUtil::ShapeShader;

  PatcherUpgradeParallaxToCM(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  auto transform(const PatcherShader::PatcherMatch &FromMatch) -> PatcherShader::PatcherMatch override;
};
