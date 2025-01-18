#pragma once

#include <DirectXTex.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"

constexpr unsigned NUM_GPU_THREADS = 16;
constexpr unsigned GPU_BUFFER_SIZE_MULTIPLE = 16;
constexpr unsigned MAX_CHANNEL_VALUE = 255;

class ParallaxGenD3D {
private:
    ParallaxGenDirectory* m_pgd;

    std::filesystem::path m_outputDir;
    std::filesystem::path m_exePath;
    bool m_useGPU;

    // GPU objects
    Microsoft::WRL::ComPtr<ID3D11Device> m_ptrDevice; // GPU device
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_ptrContext; // GPU context

    static inline const D3D_FEATURE_LEVEL s_featureLevel = D3D_FEATURE_LEVEL_11_0; // DX11

    // Each shader gets some things defined here
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_shaderMergeToComplexMaterial;
    struct ShaderMergeToComplexMaterialParams {
        DirectX::XMFLOAT2 envMapScalingFactor;
        DirectX::XMFLOAT2 parallaxMapScalingFactor;
        BOOL envMapAvailable;
        BOOL parallaxMapAvailable;
        UINT intScalingFactor;
    };
    struct ShaderMergeToComplexMaterialOutputBuffer {
        UINT minEnvValue;
        UINT maxEnvValue;
        UINT minParallaxValue;
        UINT maxParallaxValue;
    };
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_shaderCountAlphaValues;

    std::unordered_map<std::filesystem::path, DirectX::TexMetadata> m_ddsMetaDataCache;
    std::mutex m_ddsMetaDataMutex;

    std::mutex m_upgradeCMMutex;

public:
    /// @brief Constructor
    /// @param pgd ParallaxGenDirectory, must be populated before calling member functions
    /// @param outputDir absolute path of output directory
    /// @param exePath absolute path of executable directory
    /// @param useGPU if the GPU should be used, currently only findCMMaps has a corresponding CPU implementation
    ParallaxGenD3D(
        ParallaxGenDirectory* pgd, std::filesystem::path outputDir, std::filesystem::path exePath, const bool& useGPU);

    /// @brief Initialize GPU (also compiles shaders)
    void initGPU();

    /// @brief Find complex material maps and re-assign the type in the used ParallaxGenDirectory
    /// @param[in] bsaExcludes never assume files found in these BSAs are complex material maps
    /// @return result of the operation
    auto findCMMaps(const std::vector<std::wstring>& bsaExcludes) -> ParallaxGenTask::PGResult;

    /// @brief Create a complex material map from the given textures
    /// @param parallaxMap relative path to the height map in the data directory
    /// @param envMap relative path to the env map mask in the data directory
    /// @return DirectX scratch image, format DXGI_FORMAT_BC3_UNORM on success, otherwise empty ScratchImage
    [[nodiscard]] auto upgradeToComplexMaterial(
        const std::filesystem::path& parallaxMap, const std::filesystem::path& envMap) -> DirectX::ScratchImage;

    /// @brief Checks if the aspect ratio of two DDS files match
    /// @param[in] ddsPath1 relative path of the first file in the data directory
    /// @param[in] ddsPath2 relative path of the second file in the data directory
    /// @return if the aspect ratio matches
    auto checkIfAspectRatioMatches(const std::filesystem::path& ddsPath1, const std::filesystem::path& ddsPath2)
        -> bool;

    /// @brief  Gets the error message from an HRESULT for logging
    /// @param hr the HRESULT to check
    /// @return error message
    static auto getHRESULTErrorMessage(HRESULT hr) -> std::string;

private:
    auto checkIfCM(const std::filesystem::path& ddsPath, bool& result, bool& hasMetalness) -> ParallaxGenTask::PGResult;
    auto countValuesGPU(const DirectX::ScratchImage& image) -> std::array<int, 2>;

    // GPU functions
    void initShaders();

    auto compileShader(const std::filesystem::path& filename, Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob) const
        -> ParallaxGenTask::PGResult;

    auto createComputeShader(const std::wstring& shaderPath, Microsoft::WRL::ComPtr<ID3D11ComputeShader>& shaderDest)
        -> ParallaxGenTask::PGResult;

    // GPU Helpers
    auto createTexture2D(const DirectX::ScratchImage& texture, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dest) const
        -> ParallaxGenTask::PGResult;

    auto createTexture2D(Microsoft::WRL::ComPtr<ID3D11Texture2D>& existingTexture,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& dest) const -> ParallaxGenTask::PGResult;

    auto createTexture2D(D3D11_TEXTURE2D_DESC& desc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dest) const
        -> ParallaxGenTask::PGResult;

    auto createShaderResourceView(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dest) const -> ParallaxGenTask::PGResult;

    auto createUnorderedAccessView(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& dest) const -> ParallaxGenTask::PGResult;

    auto createUnorderedAccessView(const Microsoft::WRL::ComPtr<ID3D11Resource>& gpuResource,
        const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& dest) const
        -> ParallaxGenTask::PGResult;

    auto createBuffer(const void* data, D3D11_BUFFER_DESC& desc, Microsoft::WRL::ComPtr<ID3D11Buffer>& dest) const
        -> ParallaxGenTask::PGResult;

    auto createConstantBuffer(const void* data, const UINT& size, Microsoft::WRL::ComPtr<ID3D11Buffer>& dest) const
        -> ParallaxGenTask::PGResult;

    [[nodiscard]] auto blockingDispatch(UINT threadGroupCountX, UINT threadGroupCountY, UINT threadGroupCountZ) const
        -> ParallaxGenTask::PGResult;

    [[nodiscard]] auto readBack(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& gpuResource, const int& channels) const
        -> std::vector<unsigned char>;

    template <typename T>
    [[nodiscard]] auto readBack(const Microsoft::WRL::ComPtr<ID3D11Buffer>& gpuResource) const -> std::vector<T>;

    // Texture Helpers
    auto getDDS(const std::filesystem::path& ddsPath, DirectX::ScratchImage& dds) const -> ParallaxGenTask::PGResult;

    auto getDDSMetadata(const std::filesystem::path& ddsPath, DirectX::TexMetadata& ddsMeta)
        -> ParallaxGenTask::PGResult;

    static auto loadRawPixelsToScratchImage(const std::vector<unsigned char>& rawPixels, const size_t& width,
        const size_t& height, const size_t& mips, DXGI_FORMAT format) -> DirectX::ScratchImage;

    static auto isPowerOfTwo(unsigned int x) -> bool;
};
