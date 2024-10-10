#include "BethesdaGame.hpp"

#include <filesystem>

#include <gtest/gtest.h>

namespace PGTesting {
  auto getExecutableDir() -> std::filesystem::path;

  struct TestEnvGameParams {
    BethesdaGame::GameType GameType;
    std::filesystem::path GamePath;
    std::filesystem::path AppDataPath;
    std::filesystem::path DocumentPath;
  };
}  // namespace PGTesting

namespace PGTestEnvs {
  const std::filesystem::path EXEPath = PGTesting::getExecutableDir();

  // Define test environments
  const PGTesting::TestEnvGameParams TESTENVSkyrimSE = {
    BethesdaGame::GameType::SKYRIM_SE,
    EXEPath / "env\\skyrimse\\game",
    EXEPath / "env\\skyrimse\\appdata",
    EXEPath / "env\\skyrimse\\documents"
  };
}  // namespace PGTestEnvs
