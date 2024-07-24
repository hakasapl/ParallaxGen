#ifndef PARALLAXGEND3D_H
#define PARALLAXGEND3D_H

#include <d3d11.h>
#include <DirectXTex.h>
#include <wrl/client.h>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <string>

#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

class ParallaxGenD3D
{
private:
    ParallaxGenDirectory* pgd;

    std::filesystem::path output_dir;

    // GPU objects
    Microsoft::WRL::ComPtr<ID3D11Device> pDevice;           // GPU device
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;  // GPU context
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const UINT NUM_GPU_THREADS = 16;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader_MergeToComplexMaterial;
    struct shader_MergeToComplexMaterial_params
    {
        DirectX::XMFLOAT2 EnvMapScalingFactor;
        DirectX::XMFLOAT2 ParallaxMapScalingFactor;
        BOOL EnvMapAvailable;
        BOOL ParallaxMapAvailable;
        UINT intScalingFactor;
    };
    struct shader_MergeToComplexMaterial_outputbuffer
    {
        UINT MinEnvValue;
        UINT MaxEnvValue;
        UINT MinParallaxValue;
        UINT MaxParallaxValue;
    };

public:
    ParallaxGenD3D(ParallaxGenDirectory* pgd, std::filesystem::path output_dir);

    bool checkIfAspectRatioMatches(const std::filesystem::path& dds_path_1, const std::filesystem::path& dds_path_2) const;

    // GPU functions
    void initGPU();

    // Attempt to upgrade vanilla parallax to complex material
    DirectX::ScratchImage upgradeToComplexMaterial(const std::filesystem::path& parallax_map, const std::filesystem::path& env_map) const;

private:
    // GPU functions
    void initShaders();
    Microsoft::WRL::ComPtr<ID3DBlob> compileShader(const std::filesystem::path& filename);
    bool createComputeShader(const std::wstring& shader_path, Microsoft::WRL::ComPtr<ID3D11ComputeShader>& cs_dest);

    // GPU Helpers
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createTexture2D(const DirectX::ScratchImage& texture) const;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createTexture2D(Microsoft::WRL::ComPtr<ID3D11Texture2D>& existing_texture) const;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createTexture2D(D3D11_TEXTURE2D_DESC& desc) const;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> createShaderResourceView(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture) const;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> createUnorderedAccessView(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture) const;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> createUnorderedAccessView(Microsoft::WRL::ComPtr<ID3D11Resource> gpu_resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc) const;
    Microsoft::WRL::ComPtr<ID3D11Buffer> createBuffer(const void* data, D3D11_BUFFER_DESC& desc) const;
    Microsoft::WRL::ComPtr<ID3D11Buffer> createConstantBuffer(const void* data, const UINT size) const;
    bool BlockingDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) const;
    std::vector<unsigned char> readBack(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& gpu_resource, const int channels) const;
    template<typename T>
    std::vector<T> readBack(const Microsoft::WRL::ComPtr<ID3D11Buffer>& gpu_resource) const;

    // Texture Helpers
    bool getDDS(const std::filesystem::path& dds_path, DirectX::ScratchImage& dds) const;
    bool getDDSMetadata(const std::filesystem::path& dds_path, DirectX::TexMetadata& dds_meta) const;
    static DirectX::ScratchImage LoadRawPixelsToScratchImage(const std::vector<unsigned char> rawPixels, size_t width, size_t height, DXGI_FORMAT format, int channels);

    // Other Helpers
    static std::string getHRESULTErrorMessage(HRESULT hr);
};

#endif