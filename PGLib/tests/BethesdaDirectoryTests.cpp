#pragma warning(push)
#pragma warning(disable : 4834)

#include "BethesdaDirectory.hpp"
#include "BethesdaGame.hpp"
#include "CommonTests.hpp"

#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <memory>

using namespace std;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
class BethesdaDirectoryTest : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
    // Set up code for each test
    void SetUp() override
    {
        const auto& params = GetParam();

        m_bg = make_unique<BethesdaGame>(
            params.GameType, false, params.GamePath, params.AppDataPath, params.DocumentPath); // no logging
        m_bd = make_unique<BethesdaDirectory>(m_bg.get(), "", nullptr, false); // no logging
    }

    // Tear down code for each test
    void TearDown() override
    {
        // Clean up after each test
    }

    std::unique_ptr<BethesdaGame> m_bg;
    std::unique_ptr<BethesdaDirectory> m_bd;
};
// NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

TEST_P(BethesdaDirectoryTest, Initialization)
{
    const auto& params = GetParam();
    EXPECT_TRUE(std::filesystem::equivalent(m_bd->getDataPath(), params.GamePath / "Data"));
}

TEST_P(BethesdaDirectoryTest, Path)
{
    EXPECT_FALSE(m_bd->isPathAscii({ L"textures\\texture\u00a1" }));
    EXPECT_TRUE(m_bd->isPathAscii({ L"textures\\texture~" }));
    EXPECT_FALSE(m_bd->isPathAscii({ L"textures\\texture�~" }));

    EXPECT_FALSE(m_bd->isPathAscii({ "textures\\texture�~" }));
    EXPECT_TRUE(m_bd->isPathAscii({ "textures\\texture~" }));

    EXPECT_TRUE(m_bd->getAsciiPathLower({ L"Textures\\Texture" }) == L"textures\\texture");

    EXPECT_TRUE(m_bd->pathEqualityIgnoreCase({ L"Textures\\Texture" }, { L"textures\\texture" }));
    EXPECT_FALSE(m_bd->pathEqualityIgnoreCase({ L"Textures\\Texture1" }, { L"textures\\texture2" }));
    EXPECT_FALSE(m_bd->pathEqualityIgnoreCase({ L"textures\\texture1" }, { L"texture\\texture2" }));

    // path separator
    EXPECT_TRUE(m_bd->pathEqualityIgnoreCase({ L"Textures\\Texture" }, { L"textures/texture" }));
    EXPECT_FALSE(m_bd->pathEqualityIgnoreCase({ L"Textures\\Texture1" }, { L"textures/texture2" }));
    EXPECT_FALSE(m_bd->pathEqualityIgnoreCase({ L"textures\\texture1" }, { L"textures/texture2" }));

    EXPECT_TRUE(
        m_bd->checkIfAnyComponentIs(L"meshes\\landscape\\bridges\\bridge01.nif", { L"landscape", L"architecture" }));
    EXPECT_FALSE(m_bd->checkIfAnyComponentIs(L"meshes\\landscape\\bridges\\bridge01.nif", { L"architecture" }));
}

TEST_P(BethesdaDirectoryTest, Glob)
{
    const std::vector<wstring> globList { L"textures\\texture*.dds", L"meshes\\*.nif" };
    EXPECT_TRUE(m_bd->checkGlob(L"textures\\texture0.dds", globList));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\mesh0.nif", globList));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\architecture/mesh1.nif", globList));
    EXPECT_FALSE(m_bd->checkGlob(L"textures\\architecture\\texture1.dds", globList));
    EXPECT_FALSE(m_bd->checkGlob(L"scripts\\script1.pex", globList));
    EXPECT_FALSE(m_bd->checkGlob(L"textures\\texture0.png", globList));

    const std::vector<wstring> globListCfg { L"*\\cameras\\*" };
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\cameras\\camera1.nif", globListCfg));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\submeshes\\cameras\\camera1.nif", globListCfg));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\Cameras\\camera1.nif", globListCfg));
    EXPECT_FALSE(m_bd->checkGlob(L"cameras\\camera1.nif", globListCfg));
    EXPECT_FALSE(m_bd->checkGlob(L"camera1.nif", globListCfg));
    EXPECT_FALSE(m_bd->checkGlob(L"cameras1.nif", globListCfg));

    // upper case
    const std::vector<wstring> globListCfgU { L"*\\cameras\\*" };
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\cameras\\camera1.nif", globListCfgU));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\submeshes\\cameras\\camera1.nif", globListCfgU));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes\\Cameras\\camera1.nif", globListCfgU));

    // TODO: really needed on windows? forward slashes in glob
    const std::vector<wstring> globListCfgSL { L"*/Cameras/*" };
    EXPECT_TRUE(m_bd->checkGlob(L"meshes/cameras/camera1.nif", globListCfgSL));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes/submeshes/cameras/camera1.nif", globListCfgSL));
    EXPECT_TRUE(m_bd->checkGlob(L"meshes/Cameras/camera1.nif", globListCfgSL));
}

TEST_P(BethesdaDirectoryTest, Files)
{
    const std::filesystem::path bridge01Path { R"(textures\landscape\roads\bridge01.dds)" };

    const std::vector<wstring> loadOrderBSAs { L"Skyrim - Textures5.bsa", L"Skyrim - Misc.bsa",
        L"unofficial skyrim special edition patch - textures.bsa", L"Skyrim - Meshes1.bsa" };

    const std::vector<wstring> notLoadOrderBSAs { L"Skyrim - Textures0.bsa" };

    // functions throw exceptions if called before populating
    EXPECT_THROW(m_bd->isBSAFile(bridge01Path), runtime_error);
    EXPECT_THROW(m_bd->isFile(bridge01Path), runtime_error);
    EXPECT_THROW(m_bd->isLooseFile(bridge01Path), runtime_error);
    EXPECT_THROW(m_bd->isBSAFile(bridge01Path), runtime_error);

    // loose files only
    m_bd->populateFileMap(false);
    const auto& fileMapLoose = m_bd->getFileMap();
    const std::filesystem::path wrCarpet01Path { L"textures\\architecture\\whiterun\\wrcarpet01.dds" };
    EXPECT_TRUE(m_bd->isFile(wrCarpet01Path));
    EXPECT_TRUE(m_bd->isLooseFile(wrCarpet01Path));
    EXPECT_FALSE(m_bd->isBSAFile(wrCarpet01Path));

    EXPECT_TRUE(fileMapLoose.at(wrCarpet01Path).bsaFile == nullptr);

    EXPECT_TRUE(!m_bd->getFile(wrCarpet01Path, false).empty());
    EXPECT_TRUE(!m_bd->getFile(wrCarpet01Path, true).empty());

    EXPECT_FALSE(m_bd->isFileInBSA(wrCarpet01Path, loadOrderBSAs)); // no BSA was loaded, so function must return false

    // Skyrim - Textures5.bsa from env
    EXPECT_FALSE(m_bd->isFile(bridge01Path));
    EXPECT_FALSE(m_bd->isLooseFile(bridge01Path));
    EXPECT_FALSE(m_bd->isBSAFile(bridge01Path));

    EXPECT_THROW(m_bd->getFile(bridge01Path, false), runtime_error);
    EXPECT_THROW(m_bd->getFile(bridge01Path, true), runtime_error);

    EXPECT_FALSE(m_bd->isFileInBSA(bridge01Path, loadOrderBSAs));
    EXPECT_FALSE(m_bd->isFileInBSA(bridge01Path, notLoadOrderBSAs));

    // all files
    m_bd->populateFileMap(true);
    const auto& fileMap = m_bd->getFileMap();

    // Sycerscote.bsa
    EXPECT_FALSE(m_bd->isFile(L"meshes\\syerscote\\goldpot01Ã,Â°.nif"));

    // Skyrim - Textures5.bsa from env
    EXPECT_TRUE(m_bd->isFile(bridge01Path));
    EXPECT_FALSE(m_bd->isLooseFile(bridge01Path));
    EXPECT_TRUE(m_bd->isBSAFile(bridge01Path));

    EXPECT_TRUE(fileMap.at(bridge01Path).bsaFile != nullptr);
    EXPECT_TRUE(
        fileMap.at(bridge01Path).bsaFile->path.filename() == std::filesystem::path { L"Skyrim - Textures5.bsa" });
    EXPECT_TRUE(fileMap.at(bridge01Path).path == bridge01Path);

    EXPECT_TRUE(!m_bd->getFile(bridge01Path, false).empty());
    EXPECT_TRUE(!m_bd->getFile(bridge01Path, true).empty());

    EXPECT_TRUE(m_bd->isFileInBSA(bridge01Path, loadOrderBSAs));
    EXPECT_FALSE(m_bd->isFileInBSA(bridge01Path, notLoadOrderBSAs));

    // Skyrim - Misc.bsa
    const std::filesystem::path abForSwornScriptPath { L"scripts\\abforswornbriarheartscript.pex" };
    EXPECT_TRUE(m_bd->isFile(abForSwornScriptPath));
    EXPECT_FALSE(m_bd->isLooseFile(abForSwornScriptPath));
    EXPECT_TRUE(m_bd->isBSAFile(abForSwornScriptPath));

    EXPECT_TRUE(fileMap.at(abForSwornScriptPath).bsaFile != nullptr);
    EXPECT_TRUE(
        fileMap.at(abForSwornScriptPath).bsaFile->path.filename() == std::filesystem::path { L"Skyrim - Misc.bsa" });
    EXPECT_TRUE(fileMap.at(abForSwornScriptPath).path == abForSwornScriptPath);

    EXPECT_TRUE(!m_bd->getFile(abForSwornScriptPath, false).empty());
    EXPECT_TRUE(!m_bd->getFile(abForSwornScriptPath, true).empty());

    EXPECT_TRUE(m_bd->isFileInBSA(abForSwornScriptPath, loadOrderBSAs));
    EXPECT_FALSE(m_bd->isFileInBSA(abForSwornScriptPath, notLoadOrderBSAs));

    // unofficial skyrim special edition patch - textures.bsa
    const std::filesystem::path stoneQuarry01Path { L"textures\\_byoh\\clutter\\resources/stonequarry01.dds" };
    EXPECT_TRUE(m_bd->isFile(stoneQuarry01Path));
    EXPECT_FALSE(m_bd->isLooseFile(stoneQuarry01Path));
    EXPECT_TRUE(m_bd->isBSAFile(stoneQuarry01Path));

    EXPECT_TRUE(fileMap.at(stoneQuarry01Path).bsaFile != nullptr);
    EXPECT_TRUE(fileMap.at(stoneQuarry01Path).bsaFile->path.filename()
        == std::filesystem::path { L"unofficial skyrim special edition patch - textures.bsa" });
    EXPECT_TRUE(fileMap.at(stoneQuarry01Path).path == stoneQuarry01Path);

    EXPECT_TRUE(!m_bd->getFile(stoneQuarry01Path, false).empty());
    EXPECT_TRUE(!m_bd->getFile(stoneQuarry01Path, true).empty());

    EXPECT_TRUE(m_bd->isFileInBSA(stoneQuarry01Path, loadOrderBSAs));
    EXPECT_FALSE(m_bd->isFileInBSA(stoneQuarry01Path, notLoadOrderBSAs));

    // Skyrim - Meshes1.bsa
    const std::filesystem::path road3way01Path { L"meshes\\landscape\\roads\\road3way01.nif" };
    EXPECT_TRUE(m_bd->isFile(road3way01Path));
    EXPECT_FALSE(m_bd->isLooseFile(road3way01Path));
    EXPECT_TRUE(m_bd->isBSAFile(road3way01Path));

    EXPECT_TRUE(fileMap.at(road3way01Path).bsaFile != nullptr);
    EXPECT_TRUE(
        fileMap.at(road3way01Path).bsaFile->path.filename() == std::filesystem::path { L"Skyrim - Meshes1.bsa" });
    EXPECT_TRUE(fileMap.at(road3way01Path).path == road3way01Path);

    EXPECT_TRUE(!m_bd->getFile(road3way01Path).empty());
    EXPECT_TRUE(!m_bd->getFile(road3way01Path, true).empty());

    EXPECT_TRUE(m_bd->isFileInBSA(stoneQuarry01Path, loadOrderBSAs));
    EXPECT_FALSE(m_bd->isFileInBSA(stoneQuarry01Path, notLoadOrderBSAs));

    // loose file overwriting BSA file
    EXPECT_TRUE(m_bd->isFile(wrCarpet01Path));
    EXPECT_TRUE(m_bd->isLooseFile(wrCarpet01Path));
    EXPECT_FALSE(m_bd->isBSAFile(wrCarpet01Path));

    EXPECT_TRUE(fileMap.at(wrCarpet01Path).bsaFile == nullptr);

    EXPECT_TRUE(!m_bd->getFile(wrCarpet01Path, false).empty());
    EXPECT_TRUE(!m_bd->getFile(wrCarpet01Path, true).empty());

    EXPECT_TRUE(m_bd->isFileInBSA(bridge01Path, loadOrderBSAs));

    // check if files can still be retrieved after clearing cache
    m_bd->clearCache();
    EXPECT_TRUE(!m_bd->getFile(bridge01Path).empty());
    EXPECT_TRUE(!m_bd->getFile(abForSwornScriptPath).empty());
    EXPECT_TRUE(!m_bd->getFile(stoneQuarry01Path).empty());
    EXPECT_TRUE(!m_bd->getFile(road3way01Path).empty());
    EXPECT_TRUE(!m_bd->getFile(wrCarpet01Path).empty());

    // file from Skyrim - Textures0, not in env
    const std::filesystem::path alduinPath { L"textures\\actors\\alduin\\alduin.dds" };
    EXPECT_FALSE(m_bd->isBSAFile(alduinPath));
    EXPECT_FALSE(m_bd->isFile(alduinPath));
    EXPECT_FALSE(m_bd->isLooseFile(alduinPath));

    // upper case path
    //  Skyrim - Textures5.bsa from env
    const std::filesystem::path bridge01PathUpper { L"TEXTURES\\LANDSCAPE\\ROADS\\bridge01.dds" };
    EXPECT_THROW(fileMap.at(bridge01PathUpper), out_of_range);
    auto bridge01PathLower = BethesdaDirectory::getAsciiPathLower(bridge01Path);
    EXPECT_TRUE(fileMap.at(bridge01PathLower).path == bridge01PathLower); // original path from BSA is lower case

    // file is same with and without cache
    auto bridgeFileWOCache = m_bd->getFile(bridge01Path, false);
    auto bridgeFileWCache = m_bd->getFile(bridge01Path, true);
    EXPECT_TRUE(bridgeFileWOCache == bridgeFileWCache);
    m_bd->clearCache();
    bridgeFileWOCache = m_bd->getFile(bridge01Path, false);
    EXPECT_TRUE(bridgeFileWOCache == bridgeFileWCache);
}

TEST_P(BethesdaDirectoryTest, LoadOrder)
{
    auto loadOrder = m_bd->getBSALoadOrder();
    EXPECT_TRUE(loadOrder.size() == 7);

    EXPECT_TRUE(std::ranges::find(loadOrder, L"Skyrim - Meshes1.bsa") != loadOrder.end());
    EXPECT_TRUE(std::ranges::find(loadOrder, L"Skyrim - Textures5.bsa") != loadOrder.end());
    EXPECT_TRUE(std::ranges::find(loadOrder, L"Skyrim - Misc.bsa") != loadOrder.end());
    EXPECT_TRUE(std::ranges::find(loadOrder, L"ccBGSSSE037-Curios.bsa") != loadOrder.end());
    EXPECT_TRUE(
        std::ranges::find(loadOrder, L"unofficial skyrim special edition patch - textures.bsa") != loadOrder.end());
    EXPECT_TRUE(std::ranges::find(loadOrder, L"unofficial skyrim special edition patch.bsa") != loadOrder.end());
}

TEST_P(BethesdaDirectoryTest, FileMap)
{
    // only loose files
    m_bd->populateFileMap(false);
    const auto& fileMapNoBSA = m_bd->getFileMap();
    EXPECT_TRUE(!fileMapNoBSA.empty());

    // all files
    m_bd->populateFileMap(true);
    const auto& fileMap = m_bd->getFileMap();

    ASSERT_TRUE(!fileMap.empty());
    cout << fileMap.size();

    // ASSERT_ is used here to stop on the first error and prevent the log spammed with thousands of error messages
    for (const auto& mapEntry : fileMap) {
        auto pathStr = mapEntry.first.wstring();

        const auto upperChars
            = std::count_if(pathStr.begin(), pathStr.end(), [](wchar_t c) { return std::iswupper(c); });
        ASSERT_TRUE(upperChars == 0);
        ASSERT_TRUE(boost::iequals(pathStr, mapEntry.second.path.wstring()));

        auto bsaFile = mapEntry.second.bsaFile;

        if (!(bsaFile == nullptr)) {
            ASSERT_TRUE(std::filesystem::exists(bsaFile->path));
            ASSERT_FALSE(m_bd->isLooseFile(mapEntry.first));
            ASSERT_TRUE(m_bd->isBSAFile(mapEntry.first));
        } else {
            ASSERT_TRUE(m_bd->isLooseFile(mapEntry.first));
        }
    }

    // clear file cache
    m_bd->clearCache();
    const auto& fileMapAfterClearCache = m_bd->getFileMap();
    EXPECT_FALSE(fileMapAfterClearCache.empty());
}

INSTANTIATE_TEST_SUITE_P(GameParametersSE, BethesdaDirectoryTest, ::testing::Values(PGTestEnvs::s_testENVSkyrimSE));

#pragma warning(pop)
