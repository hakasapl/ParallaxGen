#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>

#include "patchers/PatcherMesh.hpp"

/**
 * @class PrePatcher
 * @brief Base class for prepatchers
 */
class PatcherPre : public PatcherMesh {
public:
    // type definitions
    using PatcherPreFactory = std::function<std::unique_ptr<PatcherPre>(std::filesystem::path, nifly::NifFile*)>;
    using PatcherPreObject = std::unique_ptr<PatcherPre>;

    // Constructors
    PatcherPre(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName);
    virtual ~PatcherPre() = default;
    PatcherPre(const PatcherPre& other) = default;
    auto operator=(const PatcherPre& other) -> PatcherPre& = default;
    PatcherPre(PatcherPre&& other) noexcept = default;
    auto operator=(PatcherPre&& other) noexcept -> PatcherPre& = default;

    /**
     * @brief Apply the patch to the NIFShape if able
     *
     * @param nifShape Shape to apply patch to
     * @param nifModified Whether the NIF was modified
     * @return true Patch was applied
     * @return false Patch was not applied
     */
    virtual void applyPatch(nifly::NiShape& nifShape, bool& nifModified) = 0;
};
