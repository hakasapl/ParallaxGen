#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>

#include "patchers/Patcher.hpp"

/**
 * @class PrePatcher
 * @brief Base class for prepatchers
 */
class PatcherGlobal : public Patcher {
public:
  // type definitions
  using PatcherGlobalFactory = std::function<std::unique_ptr<PatcherGlobal>(std::filesystem::path, nifly::NifFile *)>;
  using PatcherGlobalObject = std::unique_ptr<PatcherGlobal>;

  // Constructors
  PatcherGlobal(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName);
  virtual ~PatcherGlobal() = default;
  PatcherGlobal(const PatcherGlobal &Other) = default;
  auto operator=(const PatcherGlobal &Other) -> PatcherGlobal & = default;
  PatcherGlobal(PatcherGlobal &&Other) noexcept = default;
  auto operator=(PatcherGlobal &&Other) noexcept -> PatcherGlobal & = default;

  /**
   * @brief Apply the patch to the NIFShape if able
   *
   * @param NIFShape Shape to apply patch to
   * @param NIFModified Whether the NIF was modified
   * @param ShapeDeleted Whether the shape was deleted
   * @return true Patch was applied
   * @return false Patch was not applied
   */
  virtual auto applyPatch(bool &NIFModified) -> bool = 0;
};
