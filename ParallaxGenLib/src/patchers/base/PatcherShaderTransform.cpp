#include <utility>

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/base/PatcherMeshShaderTransform.hpp"

#include <spdlog/spdlog.h>

PatcherMeshShaderTransform::PatcherMeshShaderTransform(std::filesystem::path nifPath, nifly::NifFile* nif,
    std::string patcherName, const NIFUtil::ShapeShader& from, const NIFUtil::ShapeShader& to)
    : PatcherMesh(std::move(nifPath), nif, std::move(patcherName))
    , m_fromShader(from)
    , m_toShader(to)
{
}

std::mutex PatcherMeshShaderTransform::s_errorTrackerMutex;
std::unordered_set<std::tuple<std::filesystem::path, NIFUtil::ShapeShader, NIFUtil::ShapeShader>,
    PatcherMeshShaderTransform::ErrorTrackerHasher>
    PatcherMeshShaderTransform::s_errorTracker;

void PatcherMeshShaderTransform::postError(const std::filesystem::path& file)
{
    const std::lock_guard<std::mutex> lock(s_errorTrackerMutex);

    if (s_errorTracker.insert({ file, m_fromShader, m_toShader }).second) {
        spdlog::error(L"Failed to transform from {} to {} for {}",
            ParallaxGenUtil::asciitoUTF16(NIFUtil::getStrFromShader(m_fromShader)),
            ParallaxGenUtil::asciitoUTF16(NIFUtil::getStrFromShader(m_toShader)), file.wstring());
    }
}

auto PatcherMeshShaderTransform::alreadyTried(const std::filesystem::path& file) -> bool
{
    const std::lock_guard<std::mutex> lock(s_errorTrackerMutex);

    return s_errorTracker.contains({ file, m_fromShader, m_toShader });
}
