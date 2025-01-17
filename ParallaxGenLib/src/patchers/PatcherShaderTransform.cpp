#include <utility>

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/PatcherShaderTransform.hpp"

#include <spdlog/spdlog.h>

PatcherShaderTransform::PatcherShaderTransform(std::filesystem::path nifPath, nifly::NifFile* nif,
    std::string patcherName, const NIFUtil::ShapeShader& from, const NIFUtil::ShapeShader& to)
    : Patcher(std::move(nifPath), nif, std::move(patcherName))
    , m_fromShader(from)
    , m_toShader(to)
{
}

std::mutex PatcherShaderTransform::s_errorTrackerMutex;
std::unordered_set<std::tuple<std::filesystem::path, NIFUtil::ShapeShader, NIFUtil::ShapeShader>,
    PatcherShaderTransform::ErrorTrackerHasher>
    PatcherShaderTransform::s_errorTracker;

void PatcherShaderTransform::postError(const std::filesystem::path& file)
{
    std::lock_guard<std::mutex> lock(s_errorTrackerMutex);

    if (s_errorTracker.insert({ file, m_fromShader, m_toShader }).second) {
        spdlog::error(L"Failed to transform from {} to {} for {}",
            ParallaxGenUtil::asciitoUTF16(NIFUtil::getStrFromShader(m_fromShader)),
            ParallaxGenUtil::asciitoUTF16(NIFUtil::getStrFromShader(m_toShader)), file.wstring());
    }
}

auto PatcherShaderTransform::alreadyTried(const std::filesystem::path& file) -> bool
{
    std::lock_guard<std::mutex> lock(s_errorTrackerMutex);

    return s_errorTracker.contains({ file, m_fromShader, m_toShader });
}
