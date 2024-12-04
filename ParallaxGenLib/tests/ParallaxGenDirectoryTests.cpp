#include "BethesdaGame.hpp"
#include "CommonTests.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenPlugin.hpp"

#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <memory>

using namespace std;

class ParallaxGenDirectoryTest : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
  // Set up code for each test
  void SetUp() override {
    auto &Params = GetParam();

    BG = make_unique<BethesdaGame>(Params.GameType, false, Params.GamePath, Params.AppDataPath,
                                   Params.DocumentPath); // no logging
    PGD = make_unique<ParallaxGenDirectory>(*BG, "", nullptr); // no logging
  }

  // Tear down code for each test
  void TearDown() override {
    // Clean up after each test
  }
  std::unique_ptr<BethesdaGame> BG;
  std::unique_ptr<ParallaxGenDirectory> PGD;
};

TEST_P(ParallaxGenDirectoryTest, Initialization) {
    EXPECT_THROW(PGD->mapFiles({}, {}, {}, {}), runtime_error);
}

// TODO: run with different permutations of the parameters of MultiThreading/HighMem
TEST_P(ParallaxGenDirectoryTest, MapFiles) {
  std::vector<std::wstring> ParallaxBSAExcludes{L"Skyrim - Textures5.bsa", L"ccBGSSSE037-Curios.bsa",
                                                       L"unofficial skyrim special edition patch - textures.bsa",
                                                       L"unofficial skyrim special edition patch.bsa"};

  std::vector<std::wstring> NifBlockList{L"*\\cameras\\*", L"*\\dyndolod\\*", L"*\\lod\\*", L"*\\magic\\*",
                                                L"*\\markers\\*", L"*\\mps\\*",      L"*\\sky\\*"};

  // only loose files
  bool IncludeBSAs = false;
  PGD->populateFileMap(IncludeBSAs);

  PGD->mapFiles(NifBlockList, {}, {}, ParallaxBSAExcludes);
  // check that texture from  Skyrim - Textures5.bsa is not mapped
  auto &TextureMapDiffuseLoose = PGD->getTextureMapConst(NIFUtil::TextureSlots::DIFFUSE);
  EXPECT_TRUE(TextureMapDiffuseLoose.find(L"textures\\landscape\\roads\\bridge01") == TextureMapDiffuseLoose.end());

  // all files
  IncludeBSAs = true;
  PGD->populateFileMap(IncludeBSAs);
  auto const &FileMap = PGD->getFileMap();

  bool MultiThreading = true;
  bool HighMem = true;
  bool MapFromMeshes = true;
  PGD->mapFiles(NifBlockList, {}, {}, ParallaxBSAExcludes, MapFromMeshes, MultiThreading, HighMem);

  // load all NIFs
  const auto &Meshes = PGD->getMeshes();
  EXPECT_TRUE(!Meshes.empty());

  std::map<std::filesystem::path, nifly::NifFile> NifMap;
  for (const auto &MeshPath : Meshes) {
    nifly::NifFile Nif = NIFUtil::loadNIFFromBytes(PGD->getFile(MeshPath));
    NifMap[MeshPath] = Nif;
  }

  // make sure diffuse textures are found both from BSA as well as loose files and not using bsa exclusion list
  auto &TextureMapDiffuse = PGD->getTextureMapConst(NIFUtil::TextureSlots::DIFFUSE);
  EXPECT_FALSE(TextureMapDiffuse.empty());
  int BSADiffuse = 0;
  int LooseDiffuse = 0;
  for (auto const &TextureMapEntry : TextureMapDiffuse) {
    for (auto const &Texture : TextureMapEntry.second) {
      if (PGD->isBSAFile(Texture.Path))
        BSADiffuse++;

      if (PGD->isLooseFile(Texture.Path))
        LooseDiffuse++;
    }
  }
  EXPECT_TRUE(BSADiffuse > 0);
  EXPECT_TRUE(LooseDiffuse > 0);

  // loop over all NIFs and make sure the all the texture slots have a texture in the corresponding map
  for (const auto &NifMapEntry : NifMap) {
    nifly::NifFile const &Nif = NifMapEntry.second;
    for (auto &Shape : Nif.GetShapes()) {
      if (!Shape->HasShaderProperty())
        continue;

      auto *const Shader = Nif.GetShader(Shape);
      if (Shader == nullptr)
        continue;

      if (!Shader->HasTextureSet())
        continue;

      // BACKLIGHT is not checked since it does not have a rule for naming and thus false positives are found
      for (auto TexSlot : {NIFUtil::TextureSlots::DIFFUSE, NIFUtil::TextureSlots::NORMAL, NIFUtil::TextureSlots::GLOW,
                           NIFUtil::TextureSlots::PARALLAX, NIFUtil::TextureSlots::CUBEMAP,
                           NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureSlots::MULTILAYER}) {
        string TexFile;
        Nif.GetTextureSlot(Shape, TexFile, static_cast<uint32_t>(TexSlot));
        if (TexFile.empty())
          continue;

        boost::to_lower(TexFile);

        const std::map<NIFUtil::TextureSlots, std::set<string>> suffixMap{
            {NIFUtil::TextureSlots::NORMAL, {"_n"}},       {NIFUtil::TextureSlots::GLOW, {"_g", "_sk"}},
            {NIFUtil::TextureSlots::PARALLAX, {"_p"}},     {NIFUtil::TextureSlots::CUBEMAP, {"_e"}},
            {NIFUtil::TextureSlots::ENVMASK, {"m", "em"}}, {NIFUtil::TextureSlots::MULTILAYER, {"_i"}}};

        string stem = std::filesystem::path{TexFile}.stem().string();

        // skip wrong named textures
        if (TexSlot == NIFUtil::TextureSlots::DIFFUSE) {
          if (std::any_of(suffixMap.begin(), suffixMap.end(), [&stem](auto &MapEntry) {
                return std::any_of(MapEntry.second.begin(), MapEntry.second.end(),
                                   [&stem](string suffix) { return stem.ends_with(suffix); });
              }))
            continue;
        } else {
          bool EndsWith = false;
          if (!std::any_of(suffixMap.at(TexSlot).begin(), suffixMap.at(TexSlot).end(),
                           [&stem](string suffix) { return stem.ends_with(suffix); }))
            continue;
        }

        // don't check parallax textures from ignored BSAs
        if (TexSlot == NIFUtil::TextureSlots::PARALLAX && PGD->isFileInBSA(TexFile, ParallaxBSAExcludes)) {
          continue;
        }

        // only check textures that are found in the load order
        if (PGD->isBSAFile(TexFile) || PGD->isLooseFile(TexFile)) {
          auto const &TextureMap = PGD->getTextureMapConst(TexSlot);

          // search all texture paths of the texture map for the current slot
          auto const &FoundTexture =
              std::find_if(TextureMap.begin(), TextureMap.end(), [&TexFile](auto const &TextureMapEntry) {
                return std::any_of(TextureMapEntry.second.begin(), TextureMapEntry.second.end(),
                                   [&TexFile](auto &PGTexture) { return PGTexture.Path == TexFile; });
              });

          EXPECT_TRUE(FoundTexture != TextureMap.end())
              << "TexSlot" << static_cast<uint32_t>(TexSlot) << ":" << NifMapEntry.first << " " << TexFile;

          EXPECT_TRUE(std::all_of(
              FoundTexture->second.begin(), FoundTexture->second.end(),
              [&TexSlot](auto const &PGTexture) { return (NIFUtil::getSlotFromTexType(PGTexture.Type) == TexSlot); }))
              << "TexSlot" << static_cast<uint32_t>(TexSlot) << ":" << NifMapEntry.first << " " << TexFile;
        }
      }
    }
  }

  // loose file
  const std::wstring StumpBottomForFurnitureTexture{L"textures\\smim\\clutter\\common\\stump_bottom_for_furniture"};
  EXPECT_TRUE(TextureMapDiffuse.find(StumpBottomForFurnitureTexture) != TextureMapDiffuse.end());

  auto const &StumpBottomForFurnitureTextureSet = TextureMapDiffuse.at(StumpBottomForFurnitureTexture);
  EXPECT_TRUE(StumpBottomForFurnitureTextureSet.find(
                  {L"textures\\smim\\clutter\\common\\stump_bottom_for_furniture.dds",
                   NIFUtil::TextureType::DIFFUSE}) != StumpBottomForFurnitureTextureSet.end());

  // unofficial skyrim special edition patch - textures.bsa
  const std::wstring StoneQuarryTexture{L"textures\\_byoh\\clutter\\resources\\stonequarry01"};
  EXPECT_TRUE(TextureMapDiffuse.find(StoneQuarryTexture) != TextureMapDiffuse.end());

  auto const &StoneQuarryTextureTextureSet = TextureMapDiffuse.at(StoneQuarryTexture);
  EXPECT_TRUE(StoneQuarryTextureTextureSet.find({L"textures\\_byoh\\clutter\\resources\\stonequarry01.dds",
                                                 NIFUtil::TextureType::DIFFUSE}) != StoneQuarryTextureTextureSet.end());

  EXPECT_FALSE(PGD->getTextureMapConst(NIFUtil::TextureSlots::GLOW).empty());
  EXPECT_FALSE(PGD->getTextureMapConst(NIFUtil::TextureSlots::NORMAL).empty());
  EXPECT_FALSE(PGD->getTextureMapConst(NIFUtil::TextureSlots::BACKLIGHT).empty());
  EXPECT_FALSE(PGD->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK).empty());
  EXPECT_FALSE(PGD->getTextureMapConst(NIFUtil::TextureSlots::MULTILAYER).empty());

  // no parallax textures from BSAs are included due to exclusion list
  auto &TextureMapParallax = PGD->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);
  EXPECT_FALSE(TextureMapParallax.empty());
  int BSAParallax = 0;
  int LooseParallax = 0;
  for (auto const &TextureMapEntry : TextureMapParallax) {
    for (auto const &Textures : TextureMapEntry.second) {
      if (PGD->isBSAFile(Textures.Path))
        BSAParallax++;

      if (PGD->isLooseFile(Textures.Path))
        LooseParallax++;
    }
  }
  EXPECT_TRUE(BSAParallax == 0);
  EXPECT_TRUE(LooseParallax > 0);

  EXPECT_TRUE(TextureMapDiffuse.find(StumpBottomForFurnitureTexture) != TextureMapParallax.end());
  auto const &StumpBottomForFurnitureTextureSetP = TextureMapParallax.at(StumpBottomForFurnitureTexture);
  EXPECT_TRUE(StumpBottomForFurnitureTextureSetP.find(
                  {L"textures\\smim\\clutter\\common\\stump_bottom_for_furniture_p.dds",
                   NIFUtil::TextureType::HEIGHT}) != StumpBottomForFurnitureTextureSetP.end());

  EXPECT_TRUE(PGD->getPBRJSONs().empty());
  EXPECT_TRUE(PGD->getPGJSONs().empty());
};

TEST_P(ParallaxGenDirectoryTest, Meshes) {
  // only loose files
  PGD->populateFileMap(false);
  EXPECT_TRUE(PGD->getMeshes().empty());

  // all files
  PGD->populateFileMap(true);

  std::vector<std::wstring> BSAExcludes{L"Skyrim - Textures5.bsa"};
  PGD->mapFiles({}, {}, {}, BSAExcludes);

  const auto &Meshes = PGD->getMeshes();
  EXPECT_TRUE(!Meshes.empty());
  EXPECT_TRUE(Meshes.find({L"meshes\\landscape\\roads\\road3way01.nif"}) != Meshes.end());
}

INSTANTIATE_TEST_SUITE_P(GameParametersSE, ParallaxGenDirectoryTest, ::testing::Values(PGTestEnvs::TESTENVSkyrimSE));
