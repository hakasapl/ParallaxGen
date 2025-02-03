#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>

#include "patchers/base/PatcherMesh.hpp"

/**
 * @class PrePatcher
 * @brief Base class for prepatchers
 */
class PatcherMeshGlobal : public PatcherMesh {
public:
    // type definitions
    using PatcherMeshGlobalFactory
        = std::function<std::unique_ptr<PatcherMeshGlobal>(std::filesystem::path, nifly::NifFile*)>;
    using PatcherMeshGlobalObject = std::unique_ptr<PatcherMeshGlobal>;

    // Constructors
    PatcherMeshGlobal(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName);
    virtual ~PatcherMeshGlobal() = default;
    PatcherMeshGlobal(const PatcherMeshGlobal& other) = default;
    auto operator=(const PatcherMeshGlobal& other) -> PatcherMeshGlobal& = default;
    PatcherMeshGlobal(PatcherMeshGlobal&& other) noexcept = default;
    auto operator=(PatcherMeshGlobal&& other) noexcept -> PatcherMeshGlobal& = default;

    /**
     * @brief Apply the patch to the NIFShape if able
     *
     * @param nifShape Shape to apply patch to
     * @param nifModified Whether the NIF was modified
     * @param shapeDeleted Whether the shape was deleted
     * @return true Patch was applied
     * @return false Patch was not applied
     */
    virtual auto applyPatch(bool& nifModified) -> bool = 0;
};
