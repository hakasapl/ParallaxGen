#include "NIFUtil.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <gtest/gtest.h>

TEST(NIFUtilTests, ShaderTests) {
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::COMPLEXMATERIAL), "material"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::NONE), "none"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::VANILLAPARALLAX), "parallax"));
    EXPECT_TRUE(boost::icontains(NIFUtil::getStrFromShader(NIFUtil::ShapeShader::TRUEPBR), "pbr"));

    float f1 = 1.0f;
    float f2 = 0.0f;
    for ( int i= 0; i < 10; i++)  f2 += 0.1f;
    bool Changed;
    NIFUtil::setShaderFloat(f1, f2, Changed);
    EXPECT_FALSE(Changed);

    f1 = 0.0f;
    NIFUtil::setShaderFloat(f1, 0.5f, Changed);
    EXPECT_TRUE(Changed);

    NIFUtil::setShaderFloat(f1, 0.5f, Changed);
    EXPECT_FALSE(Changed);
}

TEST(NIFUTILTests, TextureTests) {
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
              std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::COATNORMAL));
  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/inner_i.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::INNERLAYER));
  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/subsurface_s.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::MULTILAYER, NIFUtil::TextureType::SUBSURFACETINT));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/backlight_b.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::BACKLIGHT, NIFUtil::TextureType::BACKLIGHT));

  EXPECT_TRUE(NIFUtil::getDefaultsFromSuffix({"textures/diffuse.dds"}) ==
              std::make_tuple(NIFUtil::TextureSlots::UNKNOWN, NIFUtil::TextureType::UNKNOWN));
}
