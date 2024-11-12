#include "BethesdaGame.hpp"
#include "CommonTests.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenD3D.hpp"

#include <DirectXTex.h>

#include <gtest/gtest.h>

#include <memory>

using namespace std;

class ParallaxGenD3DTest : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
  // Set up code for each test
  void SetUp() override {
    auto &Params = GetParam();

    BG = make_unique<BethesdaGame>(Params.GameType, false, Params.GamePath, Params.AppDataPath,
                                   Params.DocumentPath);       // no logging
    PGD = make_unique<ParallaxGenDirectory>(*BG, "", nullptr); // no logging

    PGD3D =
        make_unique<ParallaxGenD3D>(PGD.get(), PGTestEnvs::EXEPath / "output", PGTestEnvs::EXEPath, true); // UseGPU = true
  }

  // Tear down code for each test
  void TearDown() override {
    // Clean up after each test
  }
  unique_ptr<BethesdaGame> BG;
  unique_ptr<ParallaxGenDirectory> PGD;
  unique_ptr<ParallaxGenD3D> PGD3D;
};

TEST_P(ParallaxGenD3DTest, TextureTests) { 
    const unordered_set<wstring> BSAExcludes{L"Skyrim - Textures5.bsa"};
    PGD->populateFileMap(false); // includeBSAs = false, only loose files
    PGD->mapFiles({}, {}, BSAExcludes, false); // MapFromMeshes = false, map only based on texture name for quick test runs and less dependency to PGD in this test

    EXPECT_THROW(PGD3D->findCMMaps(BSAExcludes), runtime_error);
    EXPECT_THROW(PGD3D->upgradeToComplexMaterial("", ""), runtime_error);

    PGD3D->initGPU(); 

    // make sure before conversion the type is ENVMASK and after the conversion COMPLEXMATERIAL
    auto EnvMaskMap = PGD->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);

    auto MapEntry = EnvMaskMap.find(L"textures\\dungeons\\imperial\\impdirt01");
    EXPECT_FALSE(MapEntry == EnvMaskMap.end());
    EXPECT_TRUE(MapEntry->second.find({"textures\\dungeons\\imperial\\impdirt01_m.dds",
                                       NIFUtil::TextureType::ENVIRONMENTMASK}) != MapEntry->second.end());

    MapEntry = EnvMaskMap.find(L"textures\\dungeons\\imperial\\impextwall01");
    EXPECT_FALSE(MapEntry == EnvMaskMap.end());
    EXPECT_TRUE(MapEntry->second.find({"textures\\dungeons\\imperial\\impextwall01_m.dds",
                                       NIFUtil::TextureType::ENVIRONMENTMASK}) != MapEntry->second.end());

    MapEntry = EnvMaskMap.find(L"textures\\dungeons\\imperial\\impwall06");
    EXPECT_FALSE(MapEntry == EnvMaskMap.end());
    EXPECT_TRUE(MapEntry->second.find({"textures\\dungeons\\imperial\\impwall06_m.dds",
                                       NIFUtil::TextureType::ENVIRONMENTMASK}) != MapEntry->second.end());

    bool AnyComplexMaterial = any_of(EnvMaskMap.begin(), EnvMaskMap.end(), [](auto MapEntry) {
      return any_of(MapEntry.second.begin(), MapEntry.second.end(),
                  [](auto Texture) { return(Texture.Type == NIFUtil::TextureType::COMPLEXMATERIAL); });
    });

    EXPECT_FALSE(AnyComplexMaterial);

    // start algorithm
    EXPECT_TRUE(PGD3D->findCMMaps(BSAExcludes) == ParallaxGenTask::PGResult::SUCCESS);

    EnvMaskMap = PGD->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);

    unsigned NumComplexMaterial = count_if(EnvMaskMap.begin(), EnvMaskMap.end(), [](auto MapEntry) {
      return any_of(MapEntry.second.begin(), MapEntry.second.end(),
                  [](auto Texture) { return (Texture.Type == NIFUtil::TextureType::COMPLEXMATERIAL); });
    });

    EXPECT_TRUE(NumComplexMaterial == 15);

    MapEntry = EnvMaskMap.find(L"textures\\dungeons\\imperial\\impdirt01");
    EXPECT_FALSE(MapEntry == EnvMaskMap.end());
    EXPECT_TRUE(MapEntry->second.find({"textures\\dungeons\\imperial\\impdirt01_m.dds",
                                       NIFUtil::TextureType::COMPLEXMATERIAL}) != MapEntry->second.end());

    MapEntry = EnvMaskMap.find(L"textures\\dungeons\\imperial\\impextwall01");
    EXPECT_FALSE(MapEntry == EnvMaskMap.end());
    EXPECT_TRUE(MapEntry->second.find({"textures\\dungeons\\imperial\\impextwall01_m.dds",
                                       NIFUtil::TextureType::COMPLEXMATERIAL}) != MapEntry->second.end());

    MapEntry = EnvMaskMap.find(L"textures\\dungeons\\imperial\\impwall06");
    EXPECT_FALSE(MapEntry == EnvMaskMap.end());
    EXPECT_TRUE(MapEntry->second.find({"textures\\dungeons\\imperial\\impwall06_m.dds",
                                       NIFUtil::TextureType::COMPLEXMATERIAL}) != MapEntry->second.end());

    // aspect ratio
    EXPECT_FALSE(PGD3D->checkIfAspectRatioMatches(L"textures\\clutter\\common\\rug01.dds",
                                                  L"textures\\dungeons\\imperial\\impdirt01_m.dds"));

    EXPECT_TRUE(PGD3D->checkIfAspectRatioMatches(L"textures\\clutter\\common\\rug01.dds",
                                                  L"textures\\clutter\\common\\rug01_p.dds"));

    EXPECT_TRUE(PGD3D->checkIfAspectRatioMatches(L"textures\\dungeons\\imperial\\impextwall01_m.dds",
                                                 L"textures\\dungeons\\imperial\\impdirt01_m.dds"));

    // upgrade to complex material
    auto Image = PGD3D->upgradeToComplexMaterial("", "");
    EXPECT_TRUE(Image.GetImageCount() == 0);

    filesystem::path rug01Path{L"textures\\clutter\\common\\rug01_p.dds"};

    // complex material map should not produce an output
    Image = PGD3D->upgradeToComplexMaterial("", L"textures\\dungeons\\imperial\\impdirt01_m.dds");
    EXPECT_TRUE(Image.GetImageCount() == 0);

    // load and decompress
    DirectX::ScratchImage RugImage;
    DirectX::LoadFromDDSFile((BG->getGameDataPath() / rug01Path).c_str(), DirectX::DDS_FLAGS_NONE, nullptr, RugImage);

    DirectX::ScratchImage RugImageDecompressed;
    DirectX::Decompress(RugImage.GetImages(), RugImage.GetImageCount(), RugImage.GetMetadata(),
                        DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM, RugImageDecompressed);

    Image = PGD3D->upgradeToComplexMaterial(rug01Path, "");

    constexpr unsigned RUG01_WIDTH = 256;
    constexpr unsigned RUG01_HEIGHT = 512;
    constexpr unsigned RUG01_MIPLEVELS = 10;
    EXPECT_TRUE(Image.GetMetadata().format == DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM);
    EXPECT_TRUE(Image.GetMetadata().mipLevels == RUG01_MIPLEVELS);
    EXPECT_TRUE(Image.GetMetadata().width == RUG01_WIDTH);
    EXPECT_TRUE(Image.GetMetadata().height == RUG01_HEIGHT);

    // check that rgb are all 0 and alpha is now the r value from the height map
    DirectX::ScratchImage Decompressed;
    DirectX::Decompress(Image.GetImages(), Image.GetImageCount(), Image.GetMetadata(), DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM, Decompressed);
    uint8_t *pixels = Decompressed.GetPixels();
    uint8_t *rugPixels = RugImageDecompressed.GetPixels();
    constexpr unsigned ALLWOWEDDIFFERENCE = 4; // due to compression/decompression the values are not exactly equal
    for (int i = 0; i < Decompressed.GetPixelsSize(); i += 4)
    {
      ASSERT_TRUE(pixels[i] == 0); // r
      ASSERT_TRUE(pixels[i + 1] == 0); // g
      ASSERT_TRUE(pixels[i + 2] == 0); // b
      ASSERT_TRUE(abs(pixels[i + 3] - rugPixels[i]) <= ALLWOWEDDIFFERENCE); // alpha
    }
}

INSTANTIATE_TEST_SUITE_P(GameParametersSE, ParallaxGenD3DTest, ::testing::Values(PGTestEnvs::TESTENVSkyrimSE));
