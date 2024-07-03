#ifndef PARALLAXGEND3D_H
#define PARALLAXGEND3D_H

#include <DirectXTex.h>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <string>

#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

class ParallaxGenD3D
{
private:
    ParallaxGenDirectory* pgd;

public:
    ParallaxGenD3D(ParallaxGenDirectory* pgd);

    bool CheckHeightMapMatching(const std::filesystem::path& dds_path_1, const std::filesystem::path& dds_path_2) const;

private:
    // Texture Helpers
    bool getDDS(const std::filesystem::path& dds_path, DirectX::ScratchImage& dds) const;
    bool getDDSMetadata(const std::filesystem::path& dds_path, DirectX::TexMetadata& dds_meta) const;
};

#endif