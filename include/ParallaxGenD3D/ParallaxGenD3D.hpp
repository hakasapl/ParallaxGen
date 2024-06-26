#ifndef PARALLAXGEND3D_H
#define PARALLAXGEND3D_H

#include <d3d11.h>
#include <opencv2/opencv.hpp>
#include <wrl/client.h>
#include <vector>
#include <cstddef>
#include <filesystem>

class ParallaxGenD3D
{
private:
    Microsoft::WRL::ComPtr<ID3D11Device> pDevice;           // GPU device
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;  // GPU context

    // Shaders
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> decodeShader;

    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

public:
    ParallaxGenD3D();

    cv::Mat decodeDDS(const std::vector<std::byte> dds_bytes) const;

private:
    void CreateShaders();
    static std::vector<char> LoadCompiledShader(const std::filesystem::path& filename);
};

#endif