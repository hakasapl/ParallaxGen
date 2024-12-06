#include "patchers/PatcherUpgradeParallaxToCM.hpp"

#include "Logger.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/PatcherShader.hpp"

#include <filesystem>
#include <mutex>
#include <utility>

using namespace std;

mutex PatcherUpgradeParallaxToCM::UpgradeCMMutex;

auto PatcherUpgradeParallaxToCM::getFactory() -> PatcherShaderTransform::PatcherShaderTransformFactory {
  return [](filesystem::path NIFPath, nifly::NifFile *NIF) -> PatcherShaderTransformObject {
    return make_unique<PatcherUpgradeParallaxToCM>(std::move(NIFPath), NIF);
  };
}

auto PatcherUpgradeParallaxToCM::getFromShader() -> NIFUtil::ShapeShader {
  return NIFUtil::ShapeShader::VANILLAPARALLAX;
}
auto PatcherUpgradeParallaxToCM::getToShader() -> NIFUtil::ShapeShader { return NIFUtil::ShapeShader::COMPLEXMATERIAL; }

PatcherUpgradeParallaxToCM::PatcherUpgradeParallaxToCM(std::filesystem::path NIFPath, nifly::NifFile *NIF)
    : PatcherShaderTransform(std::move(NIFPath), NIF, "UpgradeParallaxToCM", NIFUtil::ShapeShader::VANILLAPARALLAX,
                             NIFUtil::ShapeShader::COMPLEXMATERIAL) {}

auto PatcherUpgradeParallaxToCM::transform(const PatcherShader::PatcherMatch &FromMatch,
                                           PatcherShader::PatcherMatch &Result) -> bool {
  lock_guard<mutex> Lock(UpgradeCMMutex);

  const auto HeightMap = FromMatch.MatchedPath;

  Result = FromMatch;

  if (alreadyTried(HeightMap)) {
    // already tried this file
    return false;
  }

  // Get texture base (remove _p.dds)
  const auto TexBase = NIFUtil::getTexBase(HeightMap);

  const filesystem::path ComplexMap = TexBase + L"_m.dds";

  Result.MatchedPath = ComplexMap;

  if (getPGD()->isGenerated(ComplexMap)) {
    // this was already upgraded
    return true;
  }

  static const auto CMBaseMap = getPGD()->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);
  auto ExistingMask = NIFUtil::getTexMatch(TexBase, NIFUtil::TextureType::ENVIRONMENTMASK, CMBaseMap);
  filesystem::path EnvMask = filesystem::path();
  if (!ExistingMask.empty()) {
    // env mask exists, but it's not a complex material
    // TODO smarter decision here?
    EnvMask = ExistingMask[0].Path;
  }

  // upgrade to complex material
  const DirectX::ScratchImage NewComplexMap = getPGD3D()->upgradeToComplexMaterial(HeightMap, EnvMask);

  // save to file
  if (NewComplexMap.GetImageCount() > 0) {
    const filesystem::path OutputPath = getPGD()->getGeneratedPath() / ComplexMap;
    filesystem::create_directories(OutputPath.parent_path());

    const HRESULT HR = DirectX::SaveToDDSFile(NewComplexMap.GetImages(), NewComplexMap.GetImageCount(),
                                              NewComplexMap.GetMetadata(), DirectX::DDS_FLAGS_NONE, OutputPath.c_str());
    if (FAILED(HR)) {
      Logger::debug(L"Unable to save complex material {}: {}", OutputPath.wstring(),
                    ParallaxGenUtil::ASCIItoUTF16(ParallaxGenD3D::getHRESULTErrorMessage(HR)));
      postError(HeightMap);
      return false;
    }

    // add newly created file to complexMaterialMaps for later processing
    getPGD()->getTextureMap(NIFUtil::TextureSlots::ENVMASK)[TexBase].insert(
        {ComplexMap, NIFUtil::TextureType::COMPLEXMATERIAL});
    getPGD()->setTextureType(ComplexMap, NIFUtil::TextureType::COMPLEXMATERIAL);

    // Update file map
    auto HeightMapMod = getPGD()->getMod(HeightMap);
    getPGD()->addGeneratedFile(ComplexMap, HeightMapMod);

    Logger::debug(L"Generated complex material map: {}", ComplexMap.wstring());

    return true;
  }

  postError(HeightMap);
  return false;
}
