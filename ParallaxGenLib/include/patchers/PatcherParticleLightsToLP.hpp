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
    static nlohmann::json s_lpJsonData; /** < LP JSON data */
    static std::mutex s_lpJsonDataMutex; /** < Mutex for LP JSON data */

    static constexpr int PARTICLE_LIGHT_FLAGS = 4109; /** < Particle light flags */
    static constexpr int WHITE_COLOR = 255; /** < White color */
    static constexpr double MIN_VALUE = 1e-5; /** < Minimum value */
    static constexpr float ROUNDING_VALUE = 1000000.0; /** < Rounding value */

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
     * @param nifPath NIF path to be patched
     * @param nif NIF object to be patched
     */
    PatcherParticleLightsToLP(std::filesystem::path nifPath, nifly::NifFile* nif);

    /**
     * @brief Apply this patcher to shape
     *
     * @param nifShape Shape to patch
     * @param nifModified Whether NIF was modified
     * @param shapeDeleted Whether shape was deleted
     * @return true Shape was patched
     * @return false Shape was not patched
     */
    auto applyPatch(bool& nifModified) -> bool override;

    /**
     * @brief Save output JSON
     */
    static void finalize();

private:
    /**
     * @brief Apply a single LP patch
     *
     * @param node billboard node
     * @param shape shape
     * @param effectShader effect shader
     * @return true if patch was applied
     * @return false if patch was not applied
     */
    auto applySinglePatch(
        nifly::NiBillboardNode* node, nifly::NiShape* shape, nifly::BSEffectShaderProperty* effectShader) -> bool;

    /**
     * @brief Get LP JSON for a specific NIF controller
     *
     * @param controller Controller to get JSON for
     * @param jsonField JSON field to store controller JSON in LP
     * @return nlohmann::json JSON for controller
     */
    auto getControllerJSON(nifly::NiTimeController* controller, std::string& jsonField) -> nlohmann::json;
};
