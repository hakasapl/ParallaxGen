#include "BethesdaGame.hpp"
#include "CommonTests.hpp"
#include "ParallaxGenPlugin.hpp"

#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>

#include <filesystem>
#include <memory>

using namespace std;

class BethesdaGameTest : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
  // Set up code for each test
  void SetUp() override {
    auto &Params = GetParam();

    BG = make_unique<BethesdaGame>(Params.GameType, false, Params.GamePath, Params.AppDataPath, Params.DocumentPath);
  }

  // Tear down code for each test
  void TearDown() override {
    // Clean up after each test
  }
  std::unique_ptr<BethesdaGame> BG;
};

TEST_P(BethesdaGameTest, TestCustomInitialization) {
  auto &Params = GetParam();

  EXPECT_EQ(BG->getGameType(), Params.GameType);
  EXPECT_EQ(BG->getGamePath(), Params.GamePath);
  EXPECT_TRUE(std::filesystem::equivalent(BG->getGameDataPath(), Params.GamePath / "Data"));
  //EXPECT_EQ(BG->getGameAppdataSystemPath(), Params.AppDataPath);
  //EXPECT_EQ(BG->getGameDocumentSystemPath(), Params.DocumentPath);
  EXPECT_TRUE(std::filesystem::equivalent(BG->getPluginsFile(), Params.AppDataPath / "plugins.txt"));
  EXPECT_TRUE(std::filesystem::equivalent(BG->getLoadOrderFile(), Params.AppDataPath / "loadorder.txt"));

  EXPECT_TRUE(boost::iequals(BG->getINIPaths().INI.wstring(), (Params.DocumentPath / "Skyrim.ini").wstring()));
  EXPECT_TRUE(boost::iequals(BG->getINIPaths().INIPrefs.wstring(), (Params.DocumentPath / "SkyrimPrefs.ini").wstring()));
  EXPECT_TRUE(boost::iequals(BG->getINIPaths().INICustom.wstring(), (Params.DocumentPath / "SkyrimCustom.ini").wstring()));
}

TEST_P(BethesdaGameTest, TestPlugins) {
  // Skyrim.esm
  // Update.esm
  // Dawnguard.esm
  // HearthFires.esm
  // Dragonborn.esm
  // ccBGSSSE001-Fish.esm
  // ccQDRSSE001-SurvivalMode.esl
  // ccBGSSSE037-Curios.esl
  // ccBGSSSE025-AdvDSGS.esm
  //_ResourcePack.esl
  // unofficial skyrim special edition patch.esp
  // Syerscote.esp

  EXPECT_EQ(BG->getActivePlugins().size(), 12);
  EXPECT_EQ(BG->getActivePlugins()[0], std::wstring{L"Skyrim.esm"});
  EXPECT_EQ(BG->getActivePlugins(true)[0], std::wstring{L"Skyrim"});

  EXPECT_EQ(BG->getActivePlugins()[9], std::wstring{L"_ResourcePack.esl"});
  EXPECT_EQ(BG->getActivePlugins(true)[9], std::wstring{L"_ResourcePack"});
}

INSTANTIATE_TEST_SUITE_P(GameParametersSE, BethesdaGameTest, ::testing::Values(PGTestEnvs::TESTENVSkyrimSE));
