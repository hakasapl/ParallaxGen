#include "BethesdaGame.hpp"
#include "CommonTests.hpp"

#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>

#include <filesystem>
#include <memory>

using namespace std;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
class BethesdaGameTest : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
    // Set up code for each test
    void SetUp() override
    {
        const auto& params = GetParam();

        m_bg = make_unique<BethesdaGame>(
            params.GameType, false, params.GamePath, params.AppDataPath, params.DocumentPath);
    }

    // Tear down code for each test
    void TearDown() override
    {
        // Clean up after each test
    }

    std::unique_ptr<BethesdaGame> m_bg;
};
// NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

TEST_P(BethesdaGameTest, TestCustomInitialization)
{
    const auto& params = GetParam();

    EXPECT_EQ(m_bg->getGameType(), params.GameType);
    EXPECT_EQ(m_bg->getGamePath(), params.GamePath);
    EXPECT_TRUE(std::filesystem::equivalent(m_bg->getGameDataPath(), params.GamePath / "Data"));
    // EXPECT_EQ(m_bg->getGameAppdataSystemPath(), Params.AppDataPath);
    // EXPECT_EQ(m_bg->getGameDocumentSystemPath(), Params.DocumentPath);
    EXPECT_TRUE(std::filesystem::equivalent(m_bg->getPluginsFile(), params.AppDataPath / "plugins.txt"));

    EXPECT_TRUE(boost::iequals(m_bg->getINIPaths().ini.wstring(), (params.DocumentPath / "Skyrim.ini").wstring()));
    EXPECT_TRUE(
        boost::iequals(m_bg->getINIPaths().iniPrefs.wstring(), (params.DocumentPath / "SkyrimPrefs.ini").wstring()));
    EXPECT_TRUE(
        boost::iequals(m_bg->getINIPaths().iniCustom.wstring(), (params.DocumentPath / "SkyrimCustom.ini").wstring()));
}

TEST_P(BethesdaGameTest, TestPlugins)
{
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

    EXPECT_EQ(m_bg->getActivePlugins().size(), 12);
    EXPECT_EQ(m_bg->getActivePlugins()[0], std::wstring { L"Skyrim.esm" });
    EXPECT_EQ(m_bg->getActivePlugins(true)[0], std::wstring { L"Skyrim" });

    EXPECT_EQ(m_bg->getActivePlugins()[9], std::wstring { L"_ResourcePack.esl" });
    EXPECT_EQ(m_bg->getActivePlugins(true)[9], std::wstring { L"_ResourcePack" });
}

INSTANTIATE_TEST_SUITE_P(GameParametersSE, BethesdaGameTest, ::testing::Values(PGTestEnvs::s_testENVSkyrimSE));
