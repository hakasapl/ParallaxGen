#include "BethesdaGame.hpp"
#include "CommonTests.hpp"
#include "ParallaxGenPlugin.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <boost/algorithm/string/predicate.hpp>

using namespace std;

class BethesdaGameTestSkyrimSEInstalled : public ::testing::Test {
protected:
  // Set up code for each test
  void SetUp() override {

    // must not throw exception
    BG = make_unique<BethesdaGame>(BethesdaGame::GameType::SKYRIM_SE);
  }

  // Tear down code for each test
  void TearDown() override {
    // Clean up after each test
  }
  std::unique_ptr<BethesdaGame> BG;
};

TEST_F(BethesdaGameTestSkyrimSEInstalled, TestInitialization) {
  EXPECT_EQ(BG->getGameType(), BethesdaGame::GameType::SKYRIM_SE);

  // all directories exist
  EXPECT_TRUE(std::filesystem::exists(BG->getGamePath()));
  EXPECT_TRUE(std::filesystem::exists(BG->getGameDataPath()));

  EXPECT_EQ(BG->getGameDataPath(), BG->getGamePath() / "Data");

  EXPECT_TRUE(boost::iequals(BG->getPluginsFile().filename().wstring(), L"plugins.txt"));
  EXPECT_TRUE(boost::iequals(BG->getLoadOrderFile().filename().wstring(), L"loadorder.txt"));

  EXPECT_TRUE(boost::iequals(BG->getINIPaths().INI.filename().wstring(), L"Skyrim.ini"));
  EXPECT_TRUE(boost::iequals(BG->getINIPaths().INIPrefs.filename().wstring(), L"SkyrimPrefs.ini"));
  EXPECT_TRUE(boost::iequals(BG->getINIPaths().INICustom.filename().wstring(), L"SkyrimCustom.ini"));

  EXPECT_TRUE(std::filesystem::exists(BG->getINIPaths().INI));
  EXPECT_TRUE(std::filesystem::exists(BG->getINIPaths().INIPrefs));
  EXPECT_TRUE(std::filesystem::exists(BG->getINIPaths().INICustom));
  EXPECT_TRUE(std::filesystem::exists(BG->getPluginsFile()));
  EXPECT_TRUE(std::filesystem::exists(BG->getLoadOrderFile()));
}

TEST_F(BethesdaGameTestSkyrimSEInstalled, TestPlugins) {
    EXPECT_TRUE(BG->getActivePlugins().size() > 0);

    EXPECT_EQ(BG->getActivePlugins()[0], std::wstring{L"Skyrim.esm"});
    EXPECT_EQ(BG->getActivePlugins(true)[0], std::wstring{L"Skyrim"});
}
