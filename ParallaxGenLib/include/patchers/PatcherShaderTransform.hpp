#pragma once

#include <string>

#include "patchers/Patcher.hpp"
#include "patchers/PatcherShader.hpp"

class PatcherShaderTransform : public Patcher {
public:
  using PatcherShaderTransformFactory = std::function<std::unique_ptr<PatcherShaderTransform>(std::filesystem::path, nifly::NifFile*)>;
  using PatcherShaderTransformObject = std::unique_ptr<PatcherShaderTransform>;

  // Constructors
  PatcherShaderTransform(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName);
  virtual ~PatcherShaderTransform() = default;
  PatcherShaderTransform(const PatcherShaderTransform &Other) = default;
  auto operator=(const PatcherShaderTransform &Other) -> PatcherShaderTransform & = default;
  PatcherShaderTransform(PatcherShaderTransform &&Other) noexcept = default;
  auto operator=(PatcherShaderTransform &&Other) noexcept -> PatcherShaderTransform & = default;

  virtual auto transform(const PatcherShader::PatcherMatch &FromMatch) -> PatcherShader::PatcherMatch = 0;
};
