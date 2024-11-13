#include "patchers/PatcherUpgradeParallaxToCM.hpp"

#include "Logger.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/PatcherShader.hpp"

#include <filesystem>
#include <mutex>
#include <utility>

using namespace std;

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
    : PatcherShaderTransform(std::move(NIFPath), NIF, "UpgradeParallaxToCM") {}

auto PatcherUpgradeParallaxToCM::transform(const PatcherShader::PatcherMatch &FromMatch)
    -> PatcherShader::PatcherMatch {
  lock_guard<mutex> Lock(UpgradeCMMutex);

  const auto HeightMap = FromMatch.MatchedPath;

  // Get texture base (remove _p.dds)
  const auto TexBase = NIFUtil::getTexBase(HeightMap);

  const filesystem::path ComplexMap = TexBase + L"_m.dds";

  PatcherShader::PatcherMatch Result = FromMatch;
  Result.MatchedPath = ComplexMap;

  if (getPGD()->isGenerated(ComplexMap)) {
    // this was already upgraded
    return Result;
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
      Logger::error(L"Unable to save complex material {}: {}", OutputPath.wstring(),
                    ParallaxGenUtil::strToWstr(ParallaxGenD3D::getHRESULTErrorMessage(HR)));
      return FromMatch;
    }

    // add newly created file to complexMaterialMaps for later processing
    getPGD()->getTextureMap(NIFUtil::TextureSlots::ENVMASK)[TexBase].insert(
        {ComplexMap, NIFUtil::TextureType::COMPLEXMATERIAL});

    // Update file map
    auto HeightMapMod = getPGD()->getMod(HeightMap);
    getPGD()->addGeneratedFile(ComplexMap, HeightMapMod);

    Logger::debug(L"Generated complex material map: {}", ComplexMap.wstring());

    return Result;
  }

  return FromMatch;
}
