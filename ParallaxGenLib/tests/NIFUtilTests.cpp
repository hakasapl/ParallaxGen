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
    bool Changed;
    NIFUtil::setShaderFloat(f1, f2, Changed);
    EXPECT_FALSE(Changed);

    f1 = 0.0f;
    NIFUtil::setShaderFloat(f1, 0.5f, Changed);
    EXPECT_TRUE(Changed);

    NIFUtil::setShaderFloat(f1, 0.5f, Changed);
    EXPECT_FALSE(Changed);
}

TEST(NIFUtilTests, NIFTests)
{
  EXPECT_THROW(NIFUtil::loadNIFFromBytes({}), std::runtime_error);

  // read NIF
  nifly::NifFile NIF;
  const std::filesystem::path MeshPath = PGTestEnvs::TESTENVSkyrimSE.GamePath / "data\\meshes\\architecture\\whiterun\\wrclutter\\wrruglarge01.nif";

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
  EXPECT_TRUE(
      boost::iequals(NIFUtil::getTextureSlot(&NIF, Shapes[0], NIFUtil::TextureSlots::DIFFUSE), "textures\\architecture\\whiterun\\wrcarpet01.dds"));

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

  bool Changed;
  NIFUtil::setTextureSlots(&NIF, Shapes[0], TextureSlotsEmpty, Changed);
  EXPECT_TRUE(Changed);
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

  // TODO: getSlotFromTexType
  // TODO: getDefaultTextureType
  // TODO: getTexBase
  // TODO:: getSearchPrefixes
}
