#pragma once

#include <string>
#include <unordered_set>

#include "NIFUtil.hpp"
#include "patchers/Patcher.hpp"
#include "patchers/PatcherShader.hpp"

/**
 * @class PatcherShaderTransform
 * @brief Base class for shader transform patchers
 */
class PatcherShaderTransform : public Patcher {
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
    using PatcherShaderTransformFactory
        = std::function<std::unique_ptr<PatcherShaderTransform>(std::filesystem::path, nifly::NifFile*)>;
    using PatcherShaderTransformObject = std::unique_ptr<PatcherShaderTransform>;

    // Constructors
    PatcherShaderTransform(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName,
        const NIFUtil::ShapeShader& from, const NIFUtil::ShapeShader& to);
    virtual ~PatcherShaderTransform() = default;
    PatcherShaderTransform(const PatcherShaderTransform& other) = default;
    auto operator=(const PatcherShaderTransform& other) -> PatcherShaderTransform& = default;
    PatcherShaderTransform(PatcherShaderTransform&& other) noexcept = default;
    auto operator=(PatcherShaderTransform&& other) noexcept -> PatcherShaderTransform& = default;

    /**
     * @brief Transform shader match to new shader match
     *
     * @param FromMatch shader match to transform
     * @return PatcherShader::PatcherMatch transformed match
     */
    virtual auto transform(const PatcherShader::PatcherMatch& fromMatch, PatcherShader::PatcherMatch& result) -> bool
        = 0;
};
