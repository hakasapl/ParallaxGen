#include "patchers/PatcherFixMeshLighting.hpp"
#include "NIFUtil.hpp"

#include "Logger.hpp"

using namespace std;

auto PatcherFixMeshLighting::getFactory() -> PatcherPre::PatcherPreFactory
{
    return [](const filesystem::path& nifPath, nifly::NifFile* nif) -> unique_ptr<PatcherPre> {
        return make_unique<PatcherFixMeshLighting>(nifPath, nif);
    };
}

PatcherFixMeshLighting::PatcherFixMeshLighting(std::filesystem::path nifPath, nifly::NifFile* nif)
    : PatcherPre(std::move(nifPath), nif, "FixMeshLighting")
{
}

void PatcherFixMeshLighting::applyPatch(nifly::NiShape& nifShape, bool& nifModified)
{
    auto* nifShader = getNIF()->GetShader(&nifShape);
    auto* const nifShaderBSLSP = dynamic_cast<BSLightingShaderProperty*>(nifShader);

    if (nifShaderBSLSP->softlighting > SOFTLIGHTING_MAX) {
        Logger::trace(L"Setting softlighting to 0.6 because it is too high");
        NIFUtil::setShaderFloat(nifShaderBSLSP->softlighting, SOFTLIGHTING_MAX, nifModified);
    }
}
