#include "patchers/PatcherUpgradeParallaxToCM.hpp"

#include "Logger.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/PatcherShader.hpp"

#include <filesystem>
#include <mutex>
#include <utility>

using namespace std;

mutex PatcherUpgradeParallaxToCM::s_upgradeCMMutex;

auto PatcherUpgradeParallaxToCM::getFactory() -> PatcherShaderTransform::PatcherShaderTransformFactory
{
    return [](filesystem::path nifPath, nifly::NifFile* nif) -> PatcherShaderTransformObject {
        return make_unique<PatcherUpgradeParallaxToCM>(std::move(nifPath), nif);
    };
}

auto PatcherUpgradeParallaxToCM::getFromShader() -> NIFUtil::ShapeShader
{
    return NIFUtil::ShapeShader::VANILLAPARALLAX;
}
auto PatcherUpgradeParallaxToCM::getToShader() -> NIFUtil::ShapeShader { return NIFUtil::ShapeShader::COMPLEXMATERIAL; }

PatcherUpgradeParallaxToCM::PatcherUpgradeParallaxToCM(std::filesystem::path nifPath, nifly::NifFile* nif)
    : PatcherShaderTransform(std::move(nifPath), nif, "UpgradeParallaxToCM", NIFUtil::ShapeShader::VANILLAPARALLAX,
          NIFUtil::ShapeShader::COMPLEXMATERIAL)
{
}

auto PatcherUpgradeParallaxToCM::transform(
    const PatcherShader::PatcherMatch& fromMatch, PatcherShader::PatcherMatch& result) -> bool
{
    const lock_guard<mutex> lock(s_upgradeCMMutex);

    const auto heightMap = fromMatch.matchedPath;

    result = fromMatch;

    if (alreadyTried(heightMap)) {
        // already tried this file
        return false;
    }

    // Get texture base (remove _p.dds)
    const auto texBase = NIFUtil::getTexBase(heightMap);

    const filesystem::path complexMap = texBase + L"_m.dds";

    result.matchedPath = complexMap;

    if (getPGD()->isGenerated(complexMap)) {
        // this was already upgraded
        return true;
    }

    static const auto cmBaseMap = getPGD()->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);
    auto existingMask = NIFUtil::getTexMatch(texBase, NIFUtil::TextureType::ENVIRONMENTMASK, cmBaseMap);
    filesystem::path envMask = filesystem::path();
    if (!existingMask.empty()) {
        // env mask exists, but it's not a complex material
        // TODO smarter decision here?
        envMask = existingMask[0].path;
    }

    // upgrade to complex material
    const DirectX::ScratchImage newComplexMap = getPGD3D()->upgradeToComplexMaterial(heightMap, envMask);

    // save to file
    if (newComplexMap.GetImageCount() > 0) {
        const filesystem::path outputPath = getPGD()->getGeneratedPath() / complexMap;
        filesystem::create_directories(outputPath.parent_path());

        const HRESULT hr = DirectX::SaveToDDSFile(newComplexMap.GetImages(), newComplexMap.GetImageCount(),
            newComplexMap.GetMetadata(), DirectX::DDS_FLAGS_NONE, outputPath.c_str());
        if (FAILED(hr)) {
            Logger::debug(L"Unable to save complex material {}: {}", outputPath.wstring(),
                ParallaxGenUtil::asciitoUTF16(ParallaxGenD3D::getHRESULTErrorMessage(hr)));
            postError(heightMap);
            return false;
        }

        // add newly created file to complexMaterialMaps for later processing
        getPGD()->getTextureMap(NIFUtil::TextureSlots::ENVMASK)[texBase].insert(
            { complexMap, NIFUtil::TextureType::COMPLEXMATERIAL });
        getPGD()->setTextureType(complexMap, NIFUtil::TextureType::COMPLEXMATERIAL);

        // Update file map
        auto heightMapMod = getPGD()->getMod(heightMap);
        getPGD()->addGeneratedFile(complexMap, heightMapMod);

        Logger::debug(L"Generated complex material map: {}", complexMap.wstring());

        return true;
    }

    postError(heightMap);
    return false;
}
