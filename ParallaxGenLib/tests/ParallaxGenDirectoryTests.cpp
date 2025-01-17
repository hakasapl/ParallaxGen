#include "BethesdaGame.hpp"
#include "CommonTests.hpp"
#include "ParallaxGenDirectory.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>

#include <filesystem>
#include <memory>

using namespace std;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
class ParallaxGenDirectoryTest : public ::testing::TestWithParam<PGTesting::TestEnvGameParams> {
protected:
    // Set up code for each test
    void SetUp() override
    {
        const auto& params = GetParam();

        m_bg = make_unique<BethesdaGame>(params.GameType, false, params.GamePath, params.AppDataPath,
            params.DocumentPath); // no logging
        m_pgd = make_unique<ParallaxGenDirectory>(m_bg.get(), "", nullptr); // no logging
    }

    // Tear down code for each test
    void TearDown() override
    {
        // Clean up after each test
    }

    std::unique_ptr<BethesdaGame> m_bg;
    std::unique_ptr<ParallaxGenDirectory> m_pgd;
};
// NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

TEST_P(ParallaxGenDirectoryTest, Initialization) { EXPECT_THROW(m_pgd->mapFiles({}, {}, {}, {}), runtime_error); }

// TODO: run with different permutations of the parameters of multithreading/highMem
TEST_P(ParallaxGenDirectoryTest, MapFiles)
{
    const std::vector<std::wstring> parallaxBSAExcludes { L"Skyrim - Textures5.bsa", L"ccBGSSSE037-Curios.bsa",
        L"unofficial skyrim special edition patch - textures.bsa", L"unofficial skyrim special edition patch.bsa" };

    const std::vector<std::wstring> nifBlockList { L"*\\cameras\\*", L"*\\dyndolod\\*", L"*\\lod\\*", L"*\\magic\\*",
        L"*\\markers\\*", L"*\\mps\\*", L"*\\sky\\*" };

    // only loose files
    bool includeBSAs = false;
    m_pgd->populateFileMap(includeBSAs);

    m_pgd->mapFiles(nifBlockList, {}, {}, parallaxBSAExcludes);
    // check that texture from  Skyrim - Textures5.bsa is not mapped
    const auto& textureMapDiffuseLoose = m_pgd->getTextureMapConst(NIFUtil::TextureSlots::DIFFUSE);
    EXPECT_TRUE(textureMapDiffuseLoose.find(L"textures\\landscape\\roads\\bridge01") == textureMapDiffuseLoose.end());

    // all files
    includeBSAs = true;
    m_pgd->populateFileMap(includeBSAs);

    const bool multithreading = true;
    const bool highMem = true;
    const bool mapFromMeshes = true;
    m_pgd->mapFiles(nifBlockList, {}, {}, parallaxBSAExcludes, mapFromMeshes, multithreading, highMem);

    // load all NIFs
    const auto& meshes = m_pgd->getMeshes();
    EXPECT_TRUE(!meshes.empty());

    std::map<std::filesystem::path, nifly::NifFile> nifMap;
    for (const auto& meshPath : meshes) {
        const nifly::NifFile nif = NIFUtil::loadNIFFromBytes(m_pgd->getFile(meshPath));
        nifMap[meshPath] = nif;
    }

    // make sure diffuse textures are found both from BSA as well as loose files and not using bsa exclusion list
    const auto& textureMapDiffuse = m_pgd->getTextureMapConst(NIFUtil::TextureSlots::DIFFUSE);
    EXPECT_FALSE(textureMapDiffuse.empty());
    int bsaDiffuse = 0;
    int looseDiffuse = 0;
    for (auto const& textureMapEntry : textureMapDiffuse) {
        for (auto const& texture : textureMapEntry.second) {
            if (m_pgd->isBSAFile(texture.path)) {
                bsaDiffuse++;
            }

            if (m_pgd->isLooseFile(texture.path)) {
                looseDiffuse++;
            }
        }
    }
    EXPECT_TRUE(bsaDiffuse > 0);
    EXPECT_TRUE(looseDiffuse > 0);

    // loop over all NIFs and make sure the all the texture slots have a texture in the corresponding map
    for (const auto& nifMapEntry : nifMap) {
        nifly::NifFile const& nif = nifMapEntry.second;
        for (auto& shape : nif.GetShapes()) {
            if (!shape->HasShaderProperty()) {
                continue;
            }

            auto* const shader = nif.GetShader(shape);
            if (shader == nullptr) {
                continue;
            }

            if (!shader->HasTextureSet()) {
                continue;
            }

            // BACKLIGHT is not checked since it does not have a rule for naming and thus false positives are found
            for (auto texSlot : { NIFUtil::TextureSlots::DIFFUSE, NIFUtil::TextureSlots::NORMAL,
                     NIFUtil::TextureSlots::GLOW, NIFUtil::TextureSlots::PARALLAX, NIFUtil::TextureSlots::CUBEMAP,
                     NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureSlots::MULTILAYER }) {
                string texFile;
                nif.GetTextureSlot(shape, texFile, static_cast<uint32_t>(texSlot));
                if (texFile.empty()) {
                    continue;
                }

                boost::to_lower(texFile);

                const std::map<NIFUtil::TextureSlots, std::set<string>> suffixMap {
                    { NIFUtil::TextureSlots::NORMAL, { "_n" } }, { NIFUtil::TextureSlots::GLOW, { "_g", "_sk" } },
                    { NIFUtil::TextureSlots::PARALLAX, { "_p" } }, { NIFUtil::TextureSlots::CUBEMAP, { "_e" } },
                    { NIFUtil::TextureSlots::ENVMASK, { "m", "em" } }, { NIFUtil::TextureSlots::MULTILAYER, { "_i" } }
                };

                string stem = std::filesystem::path { texFile }.stem().string();

                // skip wrong named textures
                if (texSlot == NIFUtil::TextureSlots::DIFFUSE) {
                    if (std::ranges::any_of(suffixMap, [&stem](auto& mapEntry) {
                            return std::any_of(mapEntry.second.begin(), mapEntry.second.end(),
                                [&stem](const string& suffix) { return stem.ends_with(suffix); });
                        })) {
                        continue;
                    }
                } else {
                    if (!std::any_of(suffixMap.at(texSlot).begin(), suffixMap.at(texSlot).end(),
                            [&stem](const string& suffix) { return stem.ends_with(suffix); })) {
                        continue;
                    }
                }

                // don't check parallax textures from ignored BSAs
                if (texSlot == NIFUtil::TextureSlots::PARALLAX && m_pgd->isFileInBSA(texFile, parallaxBSAExcludes)) {
                    continue;
                }

                // only check textures that are found in the load order
                if (m_pgd->isBSAFile(texFile) || m_pgd->isLooseFile(texFile)) {
                    auto const& textureMap = m_pgd->getTextureMapConst(texSlot);

                    // search all texture paths of the texture map for the current slot
                    auto const& foundTexture
                        = std::ranges::find_if(textureMap, [&texFile](auto const& textureMapEntry) {
                              return std::any_of(textureMapEntry.second.begin(), textureMapEntry.second.end(),
                                  [&texFile](auto& pgTexture) { return pgTexture.path == texFile; });
                          });

                    EXPECT_TRUE(foundTexture != textureMap.end())
                        << "TexSlot" << static_cast<uint32_t>(texSlot) << ":" << nifMapEntry.first << " " << texFile;

                    EXPECT_TRUE(std::all_of(foundTexture->second.begin(), foundTexture->second.end(),
                        [&texSlot](auto const& pgTexture) {
                            return (NIFUtil::getSlotFromTexType(pgTexture.type) == texSlot);
                        }))
                        << "TexSlot" << static_cast<uint32_t>(texSlot) << ":" << nifMapEntry.first << " " << texFile;
                }
            }
        }
    }

    // loose file
    const std::wstring stumpBottomForFurnitureTexture {
        L"textures\\smim\\clutter\\common\\stump_bottom_for_furniture"
    };
    EXPECT_TRUE(textureMapDiffuse.find(stumpBottomForFurnitureTexture) != textureMapDiffuse.end());

    auto const& stumpBottomForFurnitureTextureSet = textureMapDiffuse.at(stumpBottomForFurnitureTexture);
    EXPECT_TRUE(
        stumpBottomForFurnitureTextureSet.find(
            { L"textures\\smim\\clutter\\common\\stump_bottom_for_furniture.dds", NIFUtil::TextureType::DIFFUSE })
        != stumpBottomForFurnitureTextureSet.end());

    // unofficial skyrim special edition patch - textures.bsa
    const std::wstring stoneQuarryTexture { L"textures\\_byoh\\clutter\\resources\\stonequarry01" };
    EXPECT_TRUE(textureMapDiffuse.find(stoneQuarryTexture) != textureMapDiffuse.end());

    auto const& stoneQuarryTextureTextureSet = textureMapDiffuse.at(stoneQuarryTexture);
    EXPECT_TRUE(stoneQuarryTextureTextureSet.find(
                    { L"textures\\_byoh\\clutter\\resources\\stonequarry01.dds", NIFUtil::TextureType::DIFFUSE })
        != stoneQuarryTextureTextureSet.end());

    EXPECT_FALSE(m_pgd->getTextureMapConst(NIFUtil::TextureSlots::GLOW).empty());
    EXPECT_FALSE(m_pgd->getTextureMapConst(NIFUtil::TextureSlots::NORMAL).empty());
    EXPECT_FALSE(m_pgd->getTextureMapConst(NIFUtil::TextureSlots::BACKLIGHT).empty());
    EXPECT_FALSE(m_pgd->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK).empty());
    EXPECT_FALSE(m_pgd->getTextureMapConst(NIFUtil::TextureSlots::MULTILAYER).empty());

    // no parallax textures from BSAs are included due to exclusion list
    const auto& textureMapParallax = m_pgd->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);
    EXPECT_FALSE(textureMapParallax.empty());
    int bsaParallax = 0;
    int looseParallax = 0;
    for (auto const& textureMapEntry : textureMapParallax) {
        for (auto const& textures : textureMapEntry.second) {
            if (m_pgd->isBSAFile(textures.path)) {
                bsaParallax++;
            }

            if (m_pgd->isLooseFile(textures.path)) {
                looseParallax++;
            }
        }
    }
    EXPECT_TRUE(bsaParallax == 0);
    EXPECT_TRUE(looseParallax > 0);

    EXPECT_TRUE(textureMapDiffuse.find(stumpBottomForFurnitureTexture) != textureMapParallax.end());
    auto const& stumpBottomForFurnitureTextureSetP = textureMapParallax.at(stumpBottomForFurnitureTexture);
    EXPECT_TRUE(
        stumpBottomForFurnitureTextureSetP.find(
            { L"textures\\smim\\clutter\\common\\stump_bottom_for_furniture_p.dds", NIFUtil::TextureType::HEIGHT })
        != stumpBottomForFurnitureTextureSetP.end());

    EXPECT_TRUE(m_pgd->getPBRJSONs().empty());
};

TEST_P(ParallaxGenDirectoryTest, Meshes)
{
    // only loose files
    m_pgd->populateFileMap(false);
    EXPECT_TRUE(m_pgd->getMeshes().empty());

    // all files
    m_pgd->populateFileMap(true);

    const std::vector<std::wstring> bsaExcludes { L"Skyrim - Textures5.bsa" };
    m_pgd->mapFiles({}, {}, {}, bsaExcludes);

    const auto& meshes = m_pgd->getMeshes();
    EXPECT_TRUE(!meshes.empty());
    EXPECT_TRUE(meshes.find({ L"meshes\\landscape\\roads\\road3way01.nif" }) != meshes.end());
}

INSTANTIATE_TEST_SUITE_P(GameParametersSE, ParallaxGenDirectoryTest, ::testing::Values(PGTestEnvs::s_testENVSkyrimSE));
