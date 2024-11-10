#include "NIFUtil.hpp"
#include "CommonTests.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <gtest/gtest.h>

#include <fstream>

TEST(NIFUtilTests, ShaderTests) {
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::COMPLEXMATERIAL), "material"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::NONE), "none"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::VANILLAPARALLAX), "parallax"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::TRUEPBR), "pbr"));

    float f1 = 1.0f;
    float f2 = 0.0f;
    for ( int i= 0; i < 10; i++)  f2 += 0.1f;
    bool Changed = false;
    NIFUtil::setShaderFloat(f1, f2, Changed);
    EXPECT_FALSE(Changed);

    f1 = 0.0f;
    NIFUtil::setShaderFloat(f1, 0.5f, Changed);
    EXPECT_TRUE(Changed);

    Changed = false;
    NIFUtil::setShaderFloat(f1, 0.5f, Changed);
    EXPECT_FALSE(Changed);
}

TEST(NIFUtilTests, NIFTests) {
  EXPECT_THROW(NIFUtil::loadNIFFromBytes({}), std::runtime_error);

  // read NIF
  nifly::NifFile NIF;
  const std::filesystem::path MeshPath =
      PGTestEnvs::TESTENVSkyrimSE.GamePath / "data\\meshes\\architecture\\whiterun\\wrclutter\\wrruglarge01.nif";

  std::ifstream MeshFileStream(MeshPath, std::ios_base::binary);
  ASSERT_TRUE(MeshFileStream.is_open());
  MeshFileStream.seekg(0, std::ios::end);
  size_t length = MeshFileStream.tellg();
  MeshFileStream.seekg(0, std::ios::beg);
  std::vector<std::byte> MeshBytes(length);
  MeshFileStream.read(reinterpret_cast<char *>(MeshBytes.data()), length);
  MeshFileStream.close();

  NIF = NIFUtil::loadNIFFromBytes(MeshBytes);

  auto Shapes = NIF.GetShapes();
  EXPECT_TRUE(Shapes.size() == 2);

  // getTextureSlot
  EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&NIF, Shapes[0], NIFUtil::TextureSlots::DIFFUSE),
                             "textures\\architecture\\whiterun\\wrcarpet01.dds"));

  EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&NIF, Shapes[0], NIFUtil::TextureSlots::NORMAL),
                             "textures\\architecture\\whiterun\\wrcarpet01_n.dds"));

  EXPECT_TRUE(NIFUtil::getTextureSlot(&NIF, Shapes[0], NIFUtil::TextureSlots::GLOW).empty());

  EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&NIF, Shapes[1], NIFUtil::TextureSlots::DIFFUSE),
                             "textures\\architecture\\whiterun\\wrcarpetdetails01.dds"));

  EXPECT_TRUE(boost::iequals(NIFUtil::getTextureSlot(&NIF, Shapes[1], NIFUtil::TextureSlots::NORMAL),
                             "textures\\architecture\\whiterun\\wrcarpetdetails01_n.dds"));

  EXPECT_TRUE(NIFUtil::getTextureSlot(&NIF, Shapes[1], NIFUtil::TextureSlots::ENVMASK).empty());

  // getTextureSlots
  auto TextureSlots = NIFUtil::getTextureSlots(&NIF, Shapes[0]);

  EXPECT_TRUE(boost::iequals(TextureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)],
                             "textures\\architecture\\whiterun\\wrcarpet01.dds"));
  EXPECT_TRUE(boost::iequals(TextureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)],
                             "textures\\architecture\\whiterun\\wrcarpet01_n.dds"));
  EXPECT_TRUE(TextureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());
  EXPECT_TRUE(TextureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::UNUSED)].empty());

  // setTextureSlots
  auto TextureSlotsOld = TextureSlots;
  decltype(TextureSlotsOld) TextureSlotsEmpty{};

  bool Changed = false;
  NIFUtil::setTextureSlots(&NIF, Shapes[0], TextureSlotsEmpty, Changed);
  EXPECT_TRUE(Changed);
  Changed = false;
  NIFUtil::setTextureSlots(&NIF, Shapes[0], TextureSlotsEmpty, Changed);
  EXPECT_FALSE(Changed);

  TextureSlots = NIFUtil::getTextureSlots(&NIF, Shapes[0]);
  EXPECT_TRUE(TextureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)].empty());
  EXPECT_TRUE(TextureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)].empty());
  EXPECT_TRUE(TextureSlots[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());

  NIFUtil::setTextureSlots(&NIF, Shapes[0], TextureSlotsOld, Changed);
  EXPECT_TRUE(Changed);

  // getSearchPrefixes
  auto SearchPrefixes = NIFUtil::getSearchPrefixes(NIF, Shapes[0]);

  EXPECT_TRUE(boost::iequals(SearchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)],
                             "textures\\architecture\\whiterun\\wrcarpet01"));
  EXPECT_TRUE(boost::iequals(SearchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)],
                             "textures\\architecture\\whiterun\\wrcarpet01"));
  EXPECT_TRUE(SearchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());

  // shader flags
  auto Shader = dynamic_cast<nifly::BSShaderProperty *> (NIF.GetShader(Shapes[0]));

  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));
  Changed = false;
  NIFUtil::setShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, Changed);
  EXPECT_FALSE(Changed);

  NIFUtil::setShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_PARALLAX, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_PARALLAX));

  NIFUtil::configureShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, false, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_FALSE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

  Changed = false;
  NIFUtil::configureShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, false, Changed);
  EXPECT_FALSE(Changed);
  NIFUtil::configureShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, true, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));
  NIFUtil::clearShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS,Changed);
  EXPECT_TRUE(Changed);
  EXPECT_FALSE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));
  NIFUtil::setShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags1::SLSF1_CAST_SHADOWS));

  // flags2
  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));
  Changed = false;
  NIFUtil::setShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, Changed);
  EXPECT_FALSE(Changed);

  NIFUtil::setShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_MULTI_LAYER_PARALLAX, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_MULTI_LAYER_PARALLAX));

  NIFUtil::configureShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, false, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_FALSE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));
  Changed = false;
  NIFUtil::configureShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, false, Changed);
  EXPECT_FALSE(Changed);
  NIFUtil::configureShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, true, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));
  NIFUtil::clearShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_FALSE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));
  NIFUtil::setShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_TRUE(NIFUtil::hasShaderFlag(Shader, nifly::SkyrimShaderPropertyFlags2::SLSF2_ZBUFFER_WRITE));

  // shader type
  Changed = false;
  NIFUtil::setShaderType(Shader, nifly::BSLightingShaderPropertyShaderType::BSLSP_DEFAULT, Changed);
  EXPECT_FALSE(Changed);
  NIFUtil::setShaderType(Shader, nifly::BSLightingShaderPropertyShaderType::BSLSP_PARALLAX, Changed);
  EXPECT_TRUE(Changed);
  EXPECT_TRUE(Shader->GetShaderType() == nifly::BSLightingShaderPropertyShaderType::BSLSP_PARALLAX);
}

TEST(NIFUtilTests, TextureTests) {
  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/diffuse_d.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::DIFFUSE, NIFUtil::TextureType::DIFFUSE));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/normal_n.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::NORMAL, NIFUtil::TextureType::NORMAL));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/height_p.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::PARALLAX, NIFUtil::TextureType::HEIGHT));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/glow_g.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::GLOW, NIFUtil::TextureType::EMISSIVE));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/skintint_sk.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::GLOW, NIFUtil::TextureType::SKINTINT));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/cubemap_e.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::CUBEMAP, NIFUtil::TextureType::CUBEMAP));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/height_em.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::ENVIRONMENTMASK));
  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/height_m.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::ENVIRONMENTMASK));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/roughmetaospec_rmaos.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::ENVMASK, NIFUtil::TextureType::RMAOS));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/coatnormalroughness_cnr.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::COATNORMALROUGHNESS));
  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/inner_i.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::INNERLAYER));
  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/subsurface_s.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::SUBSURFACETINT));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/backlight_b.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::BACKLIGHT, NIFUtil::TextureType::BACKLIGHT));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/diffuse.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::UNKNOWN, NIFUtil::TextureType::UNKNOWN));

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
  EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::COATNORMALROUGHNESS) ==
              NIFUtil::TextureSlots::MULTILAYER);

  EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::BACKLIGHT) == NIFUtil::TextureSlots::BACKLIGHT);
  EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::SPECULAR) == NIFUtil::TextureSlots::BACKLIGHT);
  EXPECT_TRUE(NIFUtil::getSlotFromTexType(NIFUtil::TextureType::SUBSURFACEPBR) == NIFUtil::TextureSlots::BACKLIGHT);

  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::DIFFUSE) == NIFUtil::TextureType::DIFFUSE);
  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::NORMAL) == NIFUtil::TextureType::NORMAL);
  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::GLOW) == NIFUtil::TextureType::EMISSIVE);
  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::PARALLAX) == NIFUtil::TextureType::HEIGHT);
  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::CUBEMAP) == NIFUtil::TextureType::CUBEMAP);
  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::ENVMASK) == NIFUtil::TextureType::ENVIRONMENTMASK);
  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::MULTILAYER) ==
              NIFUtil::TextureType::SUBSURFACETINT);
  EXPECT_TRUE(NIFUtil::getDefaultTextureType(NIFUtil::TextureSlots::BACKLIGHT) == NIFUtil::TextureType::BACKLIGHT);

  // getSearchPrefixes
  std::array<std::wstring, NUM_TEXTURE_SLOTS> Slots{};
  Slots[0] = L"textures\\architecture\\whiterun\\wrcarpet01.dds";
  Slots[1] = L"textures\\architecture\\whiterun\\wrcarpet01_n.dds";

  auto SearchPrefixes = NIFUtil::getSearchPrefixes(Slots);

  EXPECT_TRUE(boost::iequals(SearchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::DIFFUSE)],
                             "textures\\architecture\\whiterun\\wrcarpet01"));
  EXPECT_TRUE(boost::iequals(SearchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::NORMAL)],
                             "textures\\architecture\\whiterun\\wrcarpet01"));
  EXPECT_TRUE(SearchPrefixes[static_cast<unsigned int>(NIFUtil::TextureSlots::GLOW)].empty());

  // getTexBase
  EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\diffuse.dds"), "textures\\diffuse"));
  EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\normal_n.dds"), "textures\\normal"));
  EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\height_p.dds"), "textures\\height"));
  EXPECT_TRUE(boost::iequals(NIFUtil::getTexBase("textures\\envmask_em.dds"), "textures\\envmask"));

  // getTexMatch
  std::map<std::wstring, std::unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> EnvMaskSearchMap{
      {L"textures\\envmask0",
       {{L"textures\\envmask0_m.dds", NIFUtil::TextureType::ENVIRONMENTMASK},
        {L"textures\\complexmaterial0_m.dds", NIFUtil::TextureType::COMPLEXMATERIAL},
        {L"textures\\rmaos0_rmaos.dds", NIFUtil::TextureType::RMAOS}}},
      {L"textures\\envmask1",
       {{L"textures\\envmask1_m.dds", NIFUtil::TextureType::ENVIRONMENTMASK},
        {L"textures\\complexmaterial1_m.dds", NIFUtil::TextureType::COMPLEXMATERIAL}}}};

  auto TexMatch = NIFUtil::getTexMatch(L"textures\\envmask0", NIFUtil::TextureType::COMPLEXMATERIAL, EnvMaskSearchMap);

  ASSERT_TRUE(TexMatch.size() == 1);
  EXPECT_TRUE(TexMatch[0].Path == L"textures\\complexmaterial0_m.dds");
  EXPECT_TRUE(TexMatch[0].Type == NIFUtil::TextureType::COMPLEXMATERIAL);

  TexMatch = NIFUtil::getTexMatch(L"textures\\envmask1", NIFUtil::TextureType::ENVIRONMENTMASK, EnvMaskSearchMap);

  ASSERT_TRUE(TexMatch.size() == 1);
  EXPECT_TRUE(TexMatch[0].Path == L"textures\\envmask1_m.dds");
  EXPECT_TRUE(TexMatch[0].Type == NIFUtil::TextureType::ENVIRONMENTMASK);
}
