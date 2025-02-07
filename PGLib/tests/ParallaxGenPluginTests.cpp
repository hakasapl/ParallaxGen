#include "BethesdaGame.hpp"
#include "CommonTests.hpp"
#include "ParallaxGenPlugin.hpp"

#include <gtest/gtest.h>

using namespace std;

class ParallaxGenPluginTests : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
    // Set up code for each test
    void SetUp() override
    {
        const auto params = GetParam();

        // Initialize necessary components for each test
        // TODO test for initialize
        const BethesdaGame bg(params.GameType, false, params.GamePath, params.AppDataPath, params.DocumentPath);
        ParallaxGenPlugin::initialize(bg, "");
    }

    // Tear down code for each test
    void TearDown() override
    {
        // Clean up after each test
    }
};

TEST_P(ParallaxGenPluginTests, TestPopulateObjs)
{
    // make sure no exceptions are thrown
    EXPECT_NO_THROW(ParallaxGenPlugin::populateObjs());
}

INSTANTIATE_TEST_SUITE_P(TestEnvs, ParallaxGenPluginTests, ::testing::Values(PGTestEnvs::s_testENVSkyrimSE));
