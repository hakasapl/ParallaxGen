#pragma once

#include <Animation.hpp>
#include <BasicTypes.hpp>
#include <Geometry.hpp>
#include <Nodes.hpp>
#include <mutex>

#include "patchers/PatcherGlobal.hpp"
#include <Shaders.hpp>

/**
 * @class PrePatcherParticleLightsToLP
 * @brief patcher to transform particle lights to LP
 */
class PatcherParticleLightsToLP : public PatcherGlobal {
private:
  static nlohmann::json LPJsonData; /** < LP JSON data */
  static std::mutex LPJsonDataMutex; /** < Mutex for LP JSON data */

public:
  /**
   * @brief Get the Factory object
   *
   * @return PatcherShaderTransform::PatcherShaderTransformFactory
   */
  static auto getFactory() -> PatcherGlobal::PatcherGlobalFactory;

  /**
   * @brief Construct a new PrePatcher Particle Lights To LP patcher
   *
   * @param NIFPath NIF path to be patched
   * @param NIF NIF object to be patched
   */
  PatcherParticleLightsToLP(std::filesystem::path NIFPath, nifly::NifFile *NIF);

  /**
   * @brief Apply this patcher to shape
   *
   * @param NIFShape Shape to patch
   * @param NIFModified Whether NIF was modified
   * @param ShapeDeleted Whether shape was deleted
   * @return true Shape was patched
   * @return false Shape was not patched
   */
  auto applyPatch(bool &NIFModified) -> bool override;

  /**
   * @brief Save output JSON
   */
  static void finalize();

private:
  /**
   * @brief Apply patch to single particle light in mesh
   *
   * @param NiAlphaProperty Alpha property to patch
   * @return true patch was applied
   * @return false patch was not applied
   */
  auto applySinglePatch(nifly::NiBillboardNode *Node, nifly::NiShape *Shape, nifly::BSEffectShaderProperty *EffectShader) -> bool;

  /**
   * @brief Get LP JSON for a specific NIF controller
   *
   * @param Controller Controller to get JSON for
   * @param JSONField JSON field to store controller JSON in LP
   * @return nlohmann::json JSON for controller
   */
  auto getControllerJSON(nifly::NiTimeController *Controller, std::string &JSONField) -> nlohmann::json;
};
