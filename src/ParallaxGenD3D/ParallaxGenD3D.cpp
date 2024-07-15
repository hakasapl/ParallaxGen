#include "ParallaxGenD3D/ParallaxGenD3D.hpp"

#include <spdlog/spdlog.h>
#include <fstream>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;

ParallaxGenD3D::ParallaxGenD3D(ParallaxGenDirectory* pgd)
{
    // Constructor
    this->pgd = pgd;
}

bool ParallaxGenD3D::checkIfAspectRatioMatches(const std::filesystem::path& dds_path_1, const std::filesystem::path& dds_path_2) const
{
    // get metadata (should only pull headers, which is much faster)
    DirectX::TexMetadata dds_image_meta_1, dds_image_meta_2;
    if (!getDDSMetadata(dds_path_1, dds_image_meta_1) || !getDDSMetadata(dds_path_2, dds_image_meta_2)) {
        spdlog::warn(L"Failed to load DDS file metadata: {} and {} (skipping)", dds_path_1.wstring(), dds_path_2.wstring());
        return false;
    }

    // calculate aspect ratios
    float aspect_ratio_1 = static_cast<float>(dds_image_meta_1.width) / static_cast<float>(dds_image_meta_1.height);
    float aspect_ratio_2 = static_cast<float>(dds_image_meta_2.width) / static_cast<float>(dds_image_meta_2.height);

    // check if aspect ratios don't match
    if (aspect_ratio_1 != aspect_ratio_2) {
        spdlog::trace("Aspect ratios don't match: {} vs {}", aspect_ratio_1, aspect_ratio_2);
        return false;
    }

    return true;
}

//
// Texture Helpers
//

bool ParallaxGenD3D::getDDS(const filesystem::path& dds_path, DirectX::ScratchImage& dds) const
{
    if (pgd->isLooseFile(dds_path)) {
        spdlog::trace(L"Reading DDS loose file {}", dds_path.wstring());
        filesystem::path full_path = pgd->getFullPath(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::LoadFromDDSFile(full_path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, dds);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file: {}", hr);
            return false;
        }
    } else {
        spdlog::trace(L"Reading DDS BSA file {}", dds_path.wstring());
        vector<std::byte> dds_bytes = pgd->getFile(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::LoadFromDDSMemory(dds_bytes.data(), dds_bytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, dds);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file from memory: {}", hr);
            return false;
        }
    }

    return true;
}

bool ParallaxGenD3D::getDDSMetadata(const filesystem::path& dds_path, DirectX::TexMetadata& dds_meta) const
{
    if (pgd->isLooseFile(dds_path)) {
        spdlog::trace(L"Reading DDS loose file metadata {}", dds_path.wstring());
        filesystem::path full_path = pgd->getFullPath(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::GetMetadataFromDDSFile(full_path.c_str(), DirectX::DDS_FLAGS_NONE, dds_meta);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file metadata: {}", hr);
            return false;
        }
    } else {
        spdlog::trace(L"Reading DDS BSA file metadata {}", dds_path.wstring());
        vector<std::byte> dds_bytes = pgd->getFile(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::GetMetadataFromDDSMemory(dds_bytes.data(), dds_bytes.size(), DirectX::DDS_FLAGS_NONE, dds_meta);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file metadata from memory: {}", hr);
            return false;
        }
    }

    return true;
}
