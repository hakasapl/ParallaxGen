#ifndef PARALLAXGEND3D_H
#define PARALLAXGEND3D_H

#include <d3d11.h>
#include <DirectXTex.h>
#include <opencv2/opencv.hpp>
#include <wrl/client.h>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <string>

#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

class ParallaxGenD3D
{
private:
    Microsoft::WRL::ComPtr<ID3D11Device> pDevice;           // GPU device
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;  // GPU context

    // Shaders
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader_GaussianBlurX;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader_GaussianBlurY;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader_Passthrough;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader_Scaling;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader_SobelEdgeDetection;

    inline static const uint num_gpu_threads = 16;

    inline static const uint int_multiplier = 1000;

    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    ParallaxGenDirectory* pgd;

public:
    ParallaxGenD3D(ParallaxGenDirectory* pgd);

    bool CheckHeightMapMatching(const std::filesystem::path& dds_path_1, const std::filesystem::path& dds_path_2) const;

    cv::Mat decodeDDS(const std::filesystem::path& dds_path) const;

private:
    void CreateShaders();
    static std::vector<char> LoadCompiledShader(const std::filesystem::path& filename);

    // GPU operations
    cv::Mat EdgeDetection(const std::filesystem::path& dds_path) const;
    double EdgeComparison(const std::filesystem::path& dds_path_1, const std::filesystem::path& dds_path_2) const;

    // GPU shader structs
    struct GaussianBlurStruct
    {
        int radius;
        float sigma;
        unsigned int width;
        unsigned int height;
    };

    struct ScalingStruct
    {
        float scale_X;
        float scale_Y;
    };

    struct SobelEdgeDetectionStruct
    {
        unsigned int width;
        unsigned int height;
    };

    // GPU shader functions
    bool dispatchShader_GaussianBlurX(
        const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& input,
        const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& output,
        const unsigned int width,
        const unsigned int height,
        const int radius,
        const float sigma
    ) const;
    bool dispatchShader_GaussianBlurY(
        const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& input,
        const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& output,
        const unsigned int width,
        const unsigned int height,
        const int radius,
        const float sigma
    ) const;
    bool dispatchShader_Passthrough(
        const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& input,
        const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& output,
        const unsigned int width,
        const unsigned int height
    ) const;
    bool dispatchShader_Scaling(
        const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& input,
        const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& output,
        const unsigned int width,
        const unsigned int height,
        const unsigned int new_width,
        const unsigned int new_height
    ) const;
    bool dispatchShader_SobelEdgeDetection(
        const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& input,
        const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& output,
        const unsigned int width,
        const unsigned int height
    ) const;

    // GPU Helpers
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createTexture2D(const DirectX::ScratchImage& texture) const;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createTexture2D(Microsoft::WRL::ComPtr<ID3D11Texture2D>& existing_texture) const;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createTexture2D(D3D11_TEXTURE2D_DESC& desc) const;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> createShaderResourceView(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture) const;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> createUnorderedAccessView(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture) const;
    Microsoft::WRL::ComPtr<ID3D11Buffer> createConstantBuffer(const void* data, const UINT size) const;
    cv::Mat readBackTexture(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& gpu_resource, int type) const;
    bool createComputeShader(const std::wstring& shader_path, Microsoft::WRL::ComPtr<ID3D11ComputeShader>& cs_dest) const;

    bool BlockingDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) const;

    // Texture Helpers
    DirectX::ScratchImage getDDS(const std::filesystem::path& dds_path) const;
    DirectX::TexMetadata getDDSMetadata(const std::filesystem::path& dds_path) const;
};

#endif