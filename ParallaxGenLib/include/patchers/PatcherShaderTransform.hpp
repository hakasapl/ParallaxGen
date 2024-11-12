#pragma once

#include <string>

#include "patchers/Patcher.hpp"
#include "patchers/PatcherShader.hpp"

/**
 * @class PatcherShaderTransform
 * @brief Base class for shader transform patchers
 */
class PatcherShaderTransform : public Patcher {
public:
  // Custom type definitions
  using PatcherShaderTransformFactory = std::function<std::unique_ptr<PatcherShaderTransform>(std::filesystem::path, nifly::NifFile*)>;
  using PatcherShaderTransformObject = std::unique_ptr<PatcherShaderTransform>;

  // Constructors
  PatcherShaderTransform(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName);
  virtual ~PatcherShaderTransform() = default;
  PatcherShaderTransform(const PatcherShaderTransform &Other) = default;
  auto operator=(const PatcherShaderTransform &Other) -> PatcherShaderTransform & = default;
  PatcherShaderTransform(PatcherShaderTransform &&Other) noexcept = default;
  auto operator=(PatcherShaderTransform &&Other) noexcept -> PatcherShaderTransform & = default;

  /**
   * @brief Transform shader match to new shader match
   *
   * @param FromMatch shader match to transform
   * @return PatcherShader::PatcherMatch transformed match
   */
  virtual auto transform(const PatcherShader::PatcherMatch &FromMatch) -> PatcherShader::PatcherMatch = 0;
};
