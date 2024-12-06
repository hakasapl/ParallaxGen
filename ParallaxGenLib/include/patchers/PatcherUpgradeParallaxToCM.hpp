#pragma once

#include "NIFUtil.hpp"
#include "patchers/PatcherShaderTransform.hpp"
#include <mutex>

/**
 * @class PatcherUpgradeParallaxToCM
 * @brief Transform patcher to upgrade Parallax to CM
 */
class PatcherUpgradeParallaxToCM : public PatcherShaderTransform {
private:
  static std::mutex UpgradeCMMutex; /** < Mutex for the upgrade to CM */

public:
  /**
   * @brief Get the Factory object for Parallax > CM transform
   *
   * @return PatcherShaderTransform::PatcherShaderTransformFactory Factory object
   */
  static auto getFactory() -> PatcherShaderTransform::PatcherShaderTransformFactory;

  /**
   * @brief Get the From Shader (Parallax)
   *
   * @return NIFUtil::ShapeShader Parallax
   */
  static auto getFromShader() -> NIFUtil::ShapeShader;

  /**
   * @brief Get the To Shader object (CM)
   *
   * @return NIFUtil::ShapeShader (CM)
   */
  static auto getToShader() -> NIFUtil::ShapeShader;

  /**
   * @brief Construct a new Patcher Upgrade Parallax To CM patcher
   *
   * @param NIFPath NIF path to be patched
   * @param NIF NIF object to be patched
   */
  PatcherUpgradeParallaxToCM(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  /**
   * @brief Transform shader match to new shader match
   *
   * @param FromMatch Match to transform
   * @return PatcherShader::PatcherMatch Transformed match
   */
  auto transform(const PatcherShader::PatcherMatch &FromMatch, PatcherShader::PatcherMatch &Result) -> bool override;
};
