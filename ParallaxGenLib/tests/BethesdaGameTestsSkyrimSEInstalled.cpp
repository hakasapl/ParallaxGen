#include "BethesdaGame.hpp"
#include "CommonTests.hpp"

#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>
#include <filesystem>
#include <memory>

using namespace std;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
class BethesdaGameTestSkyrimSEInstalled : public ::testing::Test {
protected:
    // Set up code for each test
    void SetUp() override
    {

        // must not throw exception
        m_bg = make_unique<BethesdaGame>(BethesdaGame::GameType::SKYRIM_SE);
    }

    // Tear down code for each test
    void TearDown() override
    {
        // Clean up after each test
    }
    std::unique_ptr<BethesdaGame> m_bg;
};
// NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

TEST_F(BethesdaGameTestSkyrimSEInstalled, TestInitialization)
{
    EXPECT_EQ(m_bg->getGameType(), BethesdaGame::GameType::SKYRIM_SE);

    // all directories exist
    EXPECT_TRUE(std::filesystem::exists(m_bg->getGamePath()));
    EXPECT_TRUE(std::filesystem::exists(m_bg->getGameDataPath()));

    EXPECT_EQ(m_bg->getGameDataPath(), m_bg->getGamePath() / "Data");

    EXPECT_TRUE(boost::iequals(m_bg->getPluginsFile().filename().wstring(), L"plugins.txt"));

    EXPECT_TRUE(boost::iequals(m_bg->getINIPaths().ini.filename().wstring(), L"Skyrim.ini"));
    EXPECT_TRUE(boost::iequals(m_bg->getINIPaths().iniPrefs.filename().wstring(), L"SkyrimPrefs.ini"));
    EXPECT_TRUE(boost::iequals(m_bg->getINIPaths().iniCustom.filename().wstring(), L"SkyrimCustom.ini"));

    EXPECT_TRUE(std::filesystem::exists(m_bg->getINIPaths().ini));
    EXPECT_TRUE(std::filesystem::exists(m_bg->getINIPaths().iniPrefs));
    EXPECT_TRUE(std::filesystem::exists(m_bg->getINIPaths().iniCustom));
    EXPECT_TRUE(std::filesystem::exists(m_bg->getPluginsFile()));
}

TEST_F(BethesdaGameTestSkyrimSEInstalled, TestPlugins)
{
    EXPECT_TRUE(!m_bg->getActivePlugins().empty());

    EXPECT_EQ(m_bg->getActivePlugins()[0], std::wstring { L"Skyrim.esm" });
    EXPECT_EQ(m_bg->getActivePlugins(true)[0], std::wstring { L"Skyrim" });
}
