#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>

#include "patchers/PatcherMesh.hpp"

/**
 * @class PrePatcher
 * @brief Base class for prepatchers
 */
class PatcherGlobal : public PatcherMesh {
public:
    // type definitions
    using PatcherGlobalFactory = std::function<std::unique_ptr<PatcherGlobal>(std::filesystem::path, nifly::NifFile*)>;
    using PatcherGlobalObject = std::unique_ptr<PatcherGlobal>;

    // Constructors
    PatcherGlobal(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName);
    virtual ~PatcherGlobal() = default;
    PatcherGlobal(const PatcherGlobal& other) = default;
    auto operator=(const PatcherGlobal& other) -> PatcherGlobal& = default;
    PatcherGlobal(PatcherGlobal&& other) noexcept = default;
    auto operator=(PatcherGlobal&& other) noexcept -> PatcherGlobal& = default;

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
