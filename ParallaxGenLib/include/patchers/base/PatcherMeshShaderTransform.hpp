#pragma once

#include <string>
#include <unordered_set>

#include "NIFUtil.hpp"
#include "patchers/base/PatcherMesh.hpp"
#include "patchers/base/PatcherMeshShader.hpp"

/**
 * @class PatcherMeshShaderTransform
 * @brief Base class for shader transform patchers
 */
class PatcherMeshShaderTransform : public PatcherMesh {
private:
    struct ErrorTrackerHasher {
        auto operator()(const std::tuple<std::filesystem::path, NIFUtil::ShapeShader, NIFUtil::ShapeShader>& key) const
            -> std::size_t
        {
            return std::hash<std::filesystem::path> {}(std::get<0>(key))
                ^ std::hash<NIFUtil::ShapeShader> {}(std::get<1>(key))
                ^ std::hash<NIFUtil::ShapeShader> {}(std::get<2>(key));
        }
    };

    static std::mutex s_errorTrackerMutex; /** Mutex for error tracker */
    static std::unordered_set<std::tuple<std::filesystem::path, NIFUtil::ShapeShader, NIFUtil::ShapeShader>,
        ErrorTrackerHasher>
        s_errorTracker; /** Tracks transforms that failed for error messages */
    NIFUtil::ShapeShader m_fromShader; /** Shader to transform from */
    NIFUtil::ShapeShader m_toShader; /** Shader to transform to */

protected:
    void postError(const std::filesystem::path& file); /** Post error message for transform */
    auto alreadyTried(const std::filesystem::path& file) -> bool; /** Check if transform has already been tried */

public:
    // Custom type definitions
    using PatcherMeshShaderTransformFactory
        = std::function<std::unique_ptr<PatcherMeshShaderTransform>(std::filesystem::path, nifly::NifFile*)>;
    using PatcherMeshShaderTransformObject = std::unique_ptr<PatcherMeshShaderTransform>;

    // Constructors
    PatcherMeshShaderTransform(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName,
        const NIFUtil::ShapeShader& from, const NIFUtil::ShapeShader& to);
    virtual ~PatcherMeshShaderTransform() = default;
    PatcherMeshShaderTransform(const PatcherMeshShaderTransform& other) = default;
    auto operator=(const PatcherMeshShaderTransform& other) -> PatcherMeshShaderTransform& = default;
    PatcherMeshShaderTransform(PatcherMeshShaderTransform&& other) noexcept = default;
    auto operator=(PatcherMeshShaderTransform&& other) noexcept -> PatcherMeshShaderTransform& = default;

    /**
     * @brief Transform shader match to new shader match
     *
     * @param FromMatch shader match to transform
     * @return PatcherShader::PatcherMatch transformed match
     */
    virtual auto transform(const PatcherMeshShader::PatcherMatch& fromMatch, PatcherMeshShader::PatcherMatch& result)
        -> bool
        = 0;
};
