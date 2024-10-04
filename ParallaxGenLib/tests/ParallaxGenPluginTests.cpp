#include "CommonTests.hpp"
#include "BethesdaGame.hpp"
#include "ParallaxGenPlugin.hpp"

#include <gtest/gtest.h>

using namespace std;

class ParallaxGenPluginTests : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
  // Set up code for each test
  void SetUp() override {
    auto Params = GetParam();

    // Initialize necessary components for each test
    // TODO test for initialize
    BethesdaGame BGame(Params.GameType, false, Params.GamePath, Params.AppDataPath, Params.DocumentPath);
    ParallaxGenPlugin::initialize(BGame);
  }

  // Tear down code for each test
  void TearDown() override {
    // Clean up after each test
  }
};

TEST_P(ParallaxGenPluginTests, TestPopulateObjs) {
  // make sure no exceptions are thrown
  EXPECT_NO_THROW(ParallaxGenPlugin::populateObjs());
}

INSTANTIATE_TEST_SUITE_P(TestEnvs, ParallaxGenPluginTests, ::testing::Values(PGTestEnvs::TESTENVSkyrimSE));
