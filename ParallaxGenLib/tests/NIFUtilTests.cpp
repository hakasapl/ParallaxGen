#include "CommonTests.hpp"
#include "NIFUtil.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <gtest/gtest.h>

#include <fstream>
#include <ios>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
TEST(NIFUtilTests, ShaderTests)
{
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::COMPLEXMATERIAL), "material"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::NONE), "none"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::VANILLAPARALLAX), "parallax"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::TRUEPBR), "pbr"));

    float f1 = 1.0F;
    float f2 = 0.0F;
    for (int i = 0; i < 10; i++) {
        f2 += 0.1F;
    }

    EXPECT_FALSE(NIFUtil::setShaderFloat(f1, f2));

    f1 = 0.0F;
    EXPECT_TRUE(NIFUtil::setShaderFloat(f1, 0.5F));

    EXPECT_FALSE(NIFUtil::setShaderFloat(f1, 0.5F));
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

TEST(NIFUtilTests, NIFTests)
{
    EXPECT_THROW(NIFUtil::loadNIFFromBytes({}), std::runtime_error);

    // read NIF
    nifly::NifFile nif;
    const std::filesystem::path meshPath
        = PGTestEnvs::s_testENVSkyrimSE.GamePath / R"(data\meshes\architecture\whiterun\wrclutter\wrruglarge01.nif)";

    std::ifstream meshFileStream(meshPath, std::ios_base::binary);
    ASSERT_TRUE(meshFileStream.is_open());
    meshFileStream.seekg(0, std::ios::end);
    const size_t length = meshFileStream.tellg();
    meshFileStream.seekg(0, std::ios::beg);
    std::vector<std::byte> meshBytes(length);
    meshFileStream.read(
        reinterpret_cast<char*>(meshBytes.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        static_cast<std::streamsize>(length));
    meshFileStream.close();

    nif = NIFUtil::loadNIFFromBytes(meshBytes);

    auto shapes = nif.GetShapes();
    EXPECT_TRUE(shapes.size() == 2);

    // getTextureSlot
    EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&nif, shapes[0], NIFUtil::TextureSlots::DIFFUSE),
        "textures\\architecture\\whiterun\\wrcarpet01.dds"));

    EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&nif, shapes[0], NIFUtil::TextureSlots::NORMAL),
        "textures\\architecture\\whiterun\\wrcarpet01_n.dds"));

    EXPECT_TRUE(NIFUtil::getTextureSlot(&nif, shapes[0], NIFUtil::TextureSlots::GLOW).empty());

    EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&nif, shapes[1], NIFUtil::TextureSlots::DIFFUSE),
        "textures\\architecture\\whiterun\\wrcarpetdetails01.dds"));

    EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&nif, shapes[1], NIFUtil::TextureSlots::NORMAL),
        "textures\\architecture\\whiterun\\wrcarpetdetails01_n.dds"));

    EXPECT_TRUE(NIFUtil::getTextureSlot(&nif, shapes[1], NIFUtil::TextureSlots::ENVMASK).empty());

    // getTextureSlots
    auto textureSlots = NIFUtil::getTextureSlots(&nif, shapes[0]);

    EXPECT_TRUE(boost::iequals(textureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)],
        "textures\\architecture\\whiterun\\wrcarpet01.dds"));
    EXPECT_TRUE(boost::iequals(textureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)],
        "textures\\architecture\\whiterun\\wrcarpet01_n.dds"));
    EXPECT_TRUE(textureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());
    EXPECT_TRUE(textureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::UNUSED)].empty());

    // setTextureSlots
    auto textureSlotsOld = textureSlots;
    const decltype(textureSlotsOld) textureSlotsEmpty {};

    EXPECT_TRUE(NIFUtil::setTextureSlots(&nif, shapes[0], textureSlotsEmpty));

    EXPECT_FALSE(NIFUtil::setTextureSlots(&nif, shapes[0], textureSlotsEmpty));

    textureSlots = NIFUtil::getTextureSlots(&nif, shapes[0]);
    EXPECT_TRUE(textureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)].empty());
    EXPECT_TRUE(textureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)].empty());
    EXPECT_TRUE(textureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());

    EXPECT_TRUE(NIFUtil::setTextureSlots(&nif, shapes[0], textureSlotsOld));

    // getSearchPrefixes
    auto searchPrefixes = NIFUtil::getSearchPrefixes(nif, shapes[0]);

    EXPECT_TRUE(boost::iequals(searchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)],
        "textures\\architecture\\whiterun\\wrcarpet01"));
    EXPECT_TRUE(boost::iequals(searchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)],
        "textures\\architecture\\whiterun\\wrcarpet01"));
    EXPECT_TRUE(searchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());

    // shader flags
    auto* shader = dynamic_cast<nifly::BSShaderProperty*>(nif.GetShader(shapes[0]));

    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

    EXPECT_FALSE(NIFUtil::setShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

    EXPECT_TRUE(NIFUtil::setShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_PARALLAX));
    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_PARALLAX));

    EXPECT_TRUE(NIFUtil::configureShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, false));
    EXPECT_FALSE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

    EXPECT_FALSE(NIFUtil::configureShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, false));

    EXPECT_TRUE(NIFUtil::configureShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, true));
    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

    EXPECT_TRUE(NIFUtil::clearShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));
    EXPECT_FALSE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

    EXPECT_TRUE(NIFUtil::setShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));
    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

    // flags2
    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));

    EXPECT_FALSE(NIFUtil::setShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));

    EXPECT_TRUE(NIFUtil::setShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_MULTI_LAYER_PARALLAX));
    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_MULTI_LAYER_PARALLAX));

    EXPECT_TRUE(NIFUtil::configureShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, false));
    EXPECT_FALSE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));

    EXPECT_FALSE(NIFUtil::configureShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, false));

    EXPECT_TRUE(NIFUtil::configureShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, true));
    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));

    EXPECT_TRUE(NIFUtil::clearShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));
    EXPECT_FALSE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));

    EXPECT_TRUE(NIFUtil::setShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));
    EXPECT_TRUE(NIFUtil::hasShaderFlag(shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));

    // shader type
    EXPECT_FALSE(NIFUtil::setShaderType(shader, nifly::BSLightingShaderPropertyShaderType::BSLSP_DEFAULT));

    EXPECT_TRUE(NIFUtil::setShaderType(shader, nifly::BSLightingShaderPropertyShaderType::BSLSP_PARALLAX));
    EXPECT_TRUE(shader->GetShaderType() == nifly::BSLightingShaderPropertyShaderType::BSLSP_PARALLAX);
}

TEST(NIFUtilTests, TextureTests)
{
    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/diffuse_d.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::DIFFUSE, NIFUtil::TextureType::DIFFUSE));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/normal_n.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::NORMAL, NIFUtil::TextureType::NORMAL));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/height_p.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::PARALLAX, NIFUtil::TextureType::HEIGHT));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/glow_g.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::GLOW, NIFUtil::TextureType::EMISSIVE));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/skintint_sk.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::GLOW, NIFUtil::TextureType::SKINTINT));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/cubemap_e.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::CUBEMAP, NIFUtil::TextureType::CUBEMAP));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/height_em.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::ENVIRONMENTMASK));
    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/height_m.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::ENVIRONMENTMASK));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/roughmetaospec_rmaos.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::RMAOS));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/coatnormalroughness_cnr.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::COATNORMALROUGHNESS));
    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/inner_i.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::INNERLAYER));
    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/subsurface_s.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::SUBSURFACETINT));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/backlight_b.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::BACKLIGHT, NIFUtil::TextureType::BACKLIGHT));

    EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({ "textures/diffuse.dds" })
        == std::make_tuple(NIFUtil::TextureSlots::UNKNOWN, NIFUtil::TextureType::UNKNOWN));

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::DIFFUSE) == NIFUtil::TextureSlots::DIFFUSE);

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::NORMAL) == NIFUtil::TextureSlots::NORMAL);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::MODELSPACENORMAL) == NIFUtil::TextureSlots::NORMAL);

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::EMISSIVE) == NIFUtil::TextureSlots::GLOW);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::SKINTINT) == NIFUtil::TextureSlots::GLOW);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::SUBSURFACECOLOR) == NIFUtil::TextureSlots::GLOW);

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::HEIGHT) == NIFUtil::TextureSlots::PARALLAX);

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::CUBEMAP) == NIFUtil::TextureSlots::CUBEMAP);

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::ENVIRONMENTMASK) == NIFUtil::TextureSlots::ENVMASK);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::COMPLEXMATERIAL) == NIFUtil::TextureSlots::ENVMASK);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::RMAOS) == NIFUtil::TextureSlots::ENVMASK);

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::SUBSURFACETINT) == NIFUtil::TextureSlots::MULTILAYER);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::INNERLAYER) == NIFUtil::TextureSlots::MULTILAYER);
    EXPECT_TRUE(
        NIFUtil::getSlotFromTexType(NIFUtil::TextureType::COATNORMALROUGHNESS) == NIFUtil::TextureSlots::MULTILAYER);

    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::BACKLIGHT) == NIFUtil::TextureSlots::BACKLIGHT);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::SPECULAR) == NIFUtil::TextureSlots::BACKLIGHT);
    EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::SUBSURFACEPBR) == NIFUtil::TextureSlots::BACKLIGHT);

    EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::DIFFUSE) == NIFUtil::TextureType::DIFFUSE);
    EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::NORMAL) == NIFUtil::TextureType::NORMAL);
    EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::GLOW) == NIFUtil::TextureType::EMISSIVE);
    EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::PARALLAX) == NIFUtil::TextureType::HEIGHT);
    EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::CUBEMAP) == NIFUtil::TextureType::CUBEMAP);
    EXPECT_TRUE(
        NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::ENVMASK) == NIFUtil::TextureType::ENVIRONMENTMASK);
    EXPECT_TRUE(
        NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::MULTILAYER) == NIFUtil::TextureType::SUBSURFACETINT);
    EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::BACKLIGHT) == NIFUtil::TextureType::BACKLIGHT);

    // getSearchPrefixes
    std::array<std::wstring, NUM_TEXTURE_SLOTS> slots {};
    slots[0] = L"textures\\architecture\\whiterun\\wrcarpet01.dds";
    slots[1] = L"textures\\architecture\\whiterun\\wrcarpet01_n.dds";

    auto searchPrefixes = NIFUtil::getSearchPrefixes(slots);

    EXPECT_TRUE(boost::iequals(searchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)],
        "textures\\architecture\\whiterun\\wrcarpet01"));
    EXPECT_TRUE(boost::iequals(searchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)],
        "textures\\architecture\\whiterun\\wrcarpet01"));
    EXPECT_TRUE(searchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());

    // getTexBase
    EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\diffuse.dds"), "textures\\diffuse"));
    EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\normal_n.dds"), "textures\\normal"));
    EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\height_p.dds"), "textures\\height"));
    EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\envmask_em.dds"), "textures\\envmask"));

    // getTexMatch
    const std::map<std::wstring, std::unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> envMaskSearchMap {
        { L"textures\\envmask0",
            { { .path = L"textures\\envmask0_m.dds", .type = NIFUtil::TextureType::ENVIRONMENTMASK },
                { .path = L"textures\\complexmaterial0_m.dds", .type = NIFUtil::TextureType::COMPLEXMATERIAL },
                { .path = L"textures\\rmaos0_rmaos.dds", .type = NIFUtil::TextureType::RMAOS } } },
        { L"textures\\envmask1",
            { { .path = L"textures\\envmask1_m.dds", .type = NIFUtil::TextureType::ENVIRONMENTMASK },
                { .path = L"textures\\complexmaterial1_m.dds", .type = NIFUtil::TextureType::COMPLEXMATERIAL } } }
    };

    auto texMatch
        = NIFUtil::getTexMatch(L"textures\\envmask0", NIFUtil::TextureType::COMPLEXMATERIAL, envMaskSearchMap);

    ASSERT_TRUE(texMatch.size() == 1);
    EXPECT_TRUE(texMatch[0].path == L"textures\\complexmaterial0_m.dds");
    EXPECT_TRUE(texMatch[0].type == NIFUtil::TextureType::COMPLEXMATERIAL);

    texMatch = NIFUtil::getTexMatch(L"textures\\envmask1", NIFUtil::TextureType::ENVIRONMENTMASK, envMaskSearchMap);

    ASSERT_TRUE(texMatch.size() == 1);
    EXPECT_TRUE(texMatch[0].path == L"textures\\envmask1_m.dds");
    EXPECT_TRUE(texMatch[0].type == NIFUtil::TextureType::ENVIRONMENTMASK);
}
