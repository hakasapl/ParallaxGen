#include "ParallaxGenD3D/ParallaxGenD3D.hpp"

#include <spdlog/spdlog.h>
#include <DirectXTex.h>
#include <fstream>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using Microsoft::WRL::ComPtr;

ParallaxGenD3D::ParallaxGenD3D()
{
    // Constructor

    // initialize GPU device and context
    #ifdef _DEBUG
        UINT deviceFlags = D3D11_CREATE_DEVICE_DEBUG;
    #else
        UINT deviceFlags = 0;
    #endif

    HRESULT hr;
    hr = D3D11CreateDevice(
        nullptr,                    // Adapter (configured to use default)
        D3D_DRIVER_TYPE_HARDWARE,   // User Hardware
        0,                          // Irrelevant for hardware driver type
        deviceFlags,                // Device flags (enables debug if needed)
        &featureLevel,              // Feature levels this app can support
        1,                          // Just 1 feature level (D3D11)
        D3D11_SDK_VERSION,          // Set to D3D11 SDK version
        &pDevice,                   // Sets the instance device
        nullptr,                    // Returns feature level of device created (not needed)
        &pContext                   // Sets the instance device immediate context
    );

    // check if device was found successfully
	if (FAILED(hr)) {
        spdlog::debug("D3D11 device creation failure error: {}", hr);
        spdlog::critical("Unable to find any DX11 compatible devices. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
	}
}

vector<char> ParallaxGenD3D::LoadCompiledShader(const std::filesystem::path& filename) {
    ifstream shaderFile(filename, ios::binary);
    if (!shaderFile) {
        spdlog::critical(L"Failed to load shader file {}", filename.wstring());
        ParallaxGenUtil::exitWithUserInput(1);
    }

    return vector<char>((istreambuf_iterator<char>(shaderFile)), istreambuf_iterator<char>());
}

void ParallaxGenD3D::CreateShaders()
{
    // Create shaders
    HRESULT hr;

    // DecodeBC.hlsl
	auto shader_bytes = LoadCompiledShader(L"DecodeBC.cso");
	hr = pDevice->CreateComputeShader(shader_bytes.data(), shader_bytes.size(), nullptr, &decodeShader);
    if (FAILED(hr)) {
        spdlog::debug("Failed to create compute shader: {}", hr);
        spdlog::critical("Failed to create compute shader. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
    }
}

cv::Mat ParallaxGenD3D::decodeDDS(const vector<std::byte> dds_bytes) const
{
    // Define error object
    HRESULT hr;

    // Read DDS file from memory
    DirectX::ScratchImage dds_image;
    hr = DirectX::LoadFromDDSMemory(dds_bytes.data(), dds_bytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, dds_image);
    if (FAILED(hr)) {
        spdlog::debug("Failed to load DDS file from memory: {}", hr);
        return cv::Mat();
    }

    size_t num_images = dds_image.GetImageCount();
    if (num_images <= 0) {
        spdlog::debug("DDS image is empty");
        return cv::Mat();
    }

    // Create texture on GPU
	ComPtr<ID3D11Texture2D> encodedTexture;
	hr = DirectX::CreateTexture(pDevice.Get(), dds_image.GetImages(), num_images, dds_image.GetMetadata(), reinterpret_cast<ID3D11Resource**>(encodedTexture.
    ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        spdlog::debug("Failed to create texture on GPU: {}", hr);
        return cv::Mat();
    }

    D3D11_TEXTURE2D_DESC encoded_desc;
    encodedTexture->GetDesc(&encoded_desc);

    // Create target (decoded) GPU texture
    D3D11_TEXTURE2D_DESC decoded_desc;
    encodedTexture->GetDesc(&decoded_desc);  // use desc from encoded texture as a base
    decoded_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    decoded_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

    ComPtr<ID3D11Texture2D> decodedTexture;
    hr = pDevice->CreateTexture2D(&decoded_desc, nullptr, decodedTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create decoded texture on GPU: {}", hr);
        return cv::Mat();
    }

    // Create UAV for decoded texture
    D3D11_UNORDERED_ACCESS_VIEW_DESC decoded_uav_desc = {};
    decoded_uav_desc.Format = decoded_desc.Format;
    decoded_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

	ComPtr<ID3D11UnorderedAccessView> decodedUAV;
    hr = pDevice->CreateUnorderedAccessView(decodedTexture.Get(), &decoded_uav_desc, decodedUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create UAV for decoded texture: {}", hr);
        return cv::Mat();
    }

    // Create SRV for encoded texture
	D3D11_SHADER_RESOURCE_VIEW_DESC encoded_shader_desc = {};
    encoded_shader_desc.Format = encoded_desc.Format;
    encoded_shader_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    encoded_shader_desc.Texture2D.MipLevels = 1;

	ComPtr<ID3D11ShaderResourceView> encodedSRV;
    hr = pDevice->CreateShaderResourceView(encodedTexture.Get(), &encoded_shader_desc, encodedSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create SRV for encoded texture: {}", hr);
        return cv::Mat();
    }

    // Prepare Dispatch call
    pContext->CSSetShader(decodeShader.Get(), nullptr, 0);  // set decode shader
    pContext->CSSetShaderResources(0, 1, encodedSRV.GetAddressOf());
    pContext->CSSetUnorderedAccessViews(0, 1, decodedUAV.GetAddressOf(), nullptr);

    // Dispatch the compute shader
    // Set up query to block until dispatch is complete
	ComPtr<ID3D11Query> query;
	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_EVENT;
	hr = pDevice->CreateQuery(&queryDesc, query.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create query: {}", hr);
        return cv::Mat();
    }

    // let her rip
    pContext->Dispatch((decoded_desc.Width + 7) / 8, (decoded_desc.Height + 7) / 8, 1);

	// query for blocking until completion
	pContext->End(query.Get());

	// block until dispatch is complete
	while (pContext->GetData(query.Get(), nullptr, 0, 0) == S_FALSE) {}

    // Unbind resources
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    pContext->CSSetShaderResources(0, 1, nullSRV);
    ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
    pContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    encodedTexture.Reset();  // release encoded texture
    encodedSRV.Reset();
    decodedUAV.Reset();

    // Copy decoded texture to CPU
	D3D11_TEXTURE2D_DESC staging_desc;
	decodedTexture->GetDesc(&staging_desc);
    // Enable flags for CPU access
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.BindFlags = 0;

    // create staging texture and copy resource
	ComPtr<ID3D11Texture2D> stagingTexture;
    hr = pDevice->CreateTexture2D(&staging_desc, nullptr, stagingTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create staging texture: {}", hr);
        return cv::Mat();
    }

    pContext->CopyResource(stagingTexture.Get(), decodedTexture.Get());

    decodedTexture.Reset();  // release decoded texture

    // map resource to CPU
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = pContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        spdlog::debug("Failed to map resource to CPU: {}", hr);
        return cv::Mat();
    }

    // Access the texture data from mappedResource.pData
    unsigned char* data = reinterpret_cast<unsigned char*>(mappedResource.pData);

    // Cleaup
    pContext->Unmap(stagingTexture.Get(), 0);  // cleanup map

    stagingTexture.Reset();

    pContext->Flush();  // flush GPU commands

	// save data as image to file
	cv::Mat mat(decoded_desc.Width, decoded_desc.Height, CV_8UC4, data);  // create RGBA cv::Mat

	return mat;
}
