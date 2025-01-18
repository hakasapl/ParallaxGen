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
} // namespace PGTesting

namespace PGTestEnvs {
const std::filesystem::path s_exePath = PGTesting::getExecutableDir();

// Define test environments
const PGTesting::TestEnvGameParams s_testENVSkyrimSE = { .GameType = BethesdaGame::GameType::SKYRIM_SE,
    .GamePath = s_exePath / "env\\skyrimse\\game",
    .AppDataPath = s_exePath / "env\\skyrimse\\appdata",
    .DocumentPath = s_exePath / "env\\skyrimse\\documents" };
} // namespace PGTestEnvs
