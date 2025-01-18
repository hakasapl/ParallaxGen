#include "ParallaxGenD3D.hpp"

#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"

#include <dxgiformat.h>
#include <spdlog/spdlog.h>

#include <DirectXMath.h>
#include <DirectXTex.h>

// windows
#include <windows.h>

// COM support
#include <comdef.h>

// D3D
#include <d3d11.h>

// DXGI
#include <dxgi.h>

// HLSL
#include <d3dcompiler.h>
#include <dxcapi.h>

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <string>

#include <climits>
#include <cstdlib>
#include <cstring>

#include "Logger.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using Microsoft::WRL::ComPtr;

// We need to access unions as part of certain DX11 structures
// reinterpret cast is needed often for type casting with DX11
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

ParallaxGenD3D::ParallaxGenD3D(ParallaxGenDirectory* pgd, filesystem::path outputDir, filesystem::path exePath)
    : m_pgd(pgd)
    , m_outputDir(std::move(outputDir))
    , m_exePath(std::move(exePath))
{
}

auto ParallaxGenD3D::findCMMaps(const std::vector<std::wstring>& bsaExcludes) -> ParallaxGenTask::PGResult
{
    Logger::info("Finding complex material maps");

    if (m_ptrDevice == nullptr || m_ptrContext == nullptr) {
        throw runtime_error("GPU not initialized");
    }

    auto& envMasks = m_pgd->getTextureMap(NIFUtil::TextureSlots::ENVMASK);

    ParallaxGenTask::PGResult pgResult = ParallaxGenTask::PGResult::SUCCESS;

    // loop through maps
    for (auto& envSlot : envMasks) {
        vector<tuple<NIFUtil::PGTexture, bool, bool, bool>> cmMaps;

        for (const auto& envMask : envSlot.second) {
            if (envMask.type != NIFUtil::TextureType::ENVIRONMENTMASK) {
                continue;
            }

            const bool bFileInVanillaBSA = m_pgd->isFileInBSA(envMask.path, bsaExcludes);

            bool result = false;
            bool hasMetalness = false;
            bool hasGlosiness = false;
            bool hasEnvMask = false;
            if (!bFileInVanillaBSA) {
                try {
                    ParallaxGenTask::updatePGResult(pgResult,
                        checkIfCM(envMask.path, result, hasEnvMask, hasGlosiness, hasMetalness),
                        ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
                } catch (const exception& e) {
                    spdlog::error(L"Failed to check if {} is a complex material: {}", envMask.path.wstring(),
                        asciitoUTF16(e.what()));
                    pgResult = ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS;
                    continue;
                }
            } else {
                spdlog::trace(L"Envmask {} is contained in excluded BSA - skipping complex material check",
                    envMask.path.wstring());
            }

            if (result) {
                // remove old env mask
                cmMaps.emplace_back(envMask, hasEnvMask, hasGlosiness, hasMetalness);
                spdlog::trace(L"Found complex material env mask: {}", envMask.path.wstring());
            }
        }

        // update map
        for (const auto& [cmMap, hasEnvMask, hasGlosiness, hasMetalness] : cmMaps) {
            envSlot.second.erase(cmMap);
            envSlot.second.insert({ cmMap.path, NIFUtil::TextureType::COMPLEXMATERIAL });
            m_pgd->setTextureType(cmMap.path, NIFUtil::TextureType::COMPLEXMATERIAL);

            if (hasEnvMask) {
                m_pgd->addTextureAttribute(cmMap.path, NIFUtil::TextureAttribute::CM_ENVMASK);
            }

            if (hasGlosiness) {
                m_pgd->addTextureAttribute(cmMap.path, NIFUtil::TextureAttribute::CM_GLOSSINESS);
            }

            if (hasMetalness) {
                m_pgd->addTextureAttribute(cmMap.path, NIFUtil::TextureAttribute::CM_METALNESS);
            }
        }
    }

    Logger::info("Done finding complex material env maps");

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::checkIfCM(const filesystem::path& ddsPath, bool& result, bool& hasEnvMask, bool& hasGlosiness,
    bool& hasMetalness) -> ParallaxGenTask::PGResult
{
    // get metadata (should only pull headers, which is much faster)
    DirectX::TexMetadata ddsImageMeta {};
    auto pgResult = getDDSMetadata(ddsPath, ddsImageMeta);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return pgResult;
    }

    // If Alpha is opaque move on
    if (ddsImageMeta.GetAlphaMode() == DirectX::TEX_ALPHA_MODE_OPAQUE) {
        result = false;
        return ParallaxGenTask::PGResult::SUCCESS;
    }

    // bool bcCompressed = false;
    //  Only check DDS with alpha channels
    switch (ddsImageMeta.format) {
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_BC7_TYPELESS:
        // bcCompressed = true;
        [[fallthrough]];
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        break;
    default:
        result = false;
        return ParallaxGenTask::PGResult::SUCCESS;
    }

    // Read image
    DirectX::ScratchImage image;
    pgResult = getDDS(ddsPath, image);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        spdlog::error(L"Failed to load DDS file (Skipping): {}", ddsPath.wstring());
        return pgResult;
    }

    array<int, 4> values {};
    values = countValuesGPU(image);

    const size_t numPixels = ddsImageMeta.width * ddsImageMeta.height;
    if (values[3] > numPixels / 2) {
        // check alpha
        result = false;
        return ParallaxGenTask::PGResult::SUCCESS;
    }

    if (values[0] > 0) {
        hasEnvMask = true;
    }

    if (values[1] > 0) {
        // check green
        hasGlosiness = true;
    }

    if (values[2] > 0) {
        hasMetalness = true;
    }

    result = true;
    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::countValuesGPU(const DirectX::ScratchImage& image) -> array<int, 4>
{
    if ((m_ptrContext == nullptr) || (m_ptrDevice == nullptr) || (m_shaderCountAlphaValues == nullptr)) {
        throw runtime_error("GPU not initialized");
    }

    // Create GPU texture
    ComPtr<ID3D11Texture2D> inputTex;
    if (createTexture2D(image, inputTex) != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Create SRV
    ComPtr<ID3D11ShaderResourceView> inputSRV;
    if (createShaderResourceView(inputTex, inputSRV) != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Create buffer for output
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(UINT) * 4;
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(UINT);

    array<unsigned int, 4> outputData = { 0, 0, 0, 0 };
    ComPtr<ID3D11Buffer> outputBuffer;
    if (createBuffer(outputData.data(), bufferDesc, outputBuffer) != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Create UAV for output buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 4;

    ComPtr<ID3D11UnorderedAccessView> outputBufferUAV;
    if (createUnorderedAccessView(outputBuffer, uavDesc, outputBufferUAV) != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Dispatch shader
    m_ptrContext->CSSetShader(m_shaderCountAlphaValues.Get(), nullptr, 0);
    m_ptrContext->CSSetShaderResources(0, 1, inputSRV.GetAddressOf());
    m_ptrContext->CSSetUnorderedAccessViews(0, 1, outputBufferUAV.GetAddressOf(), nullptr);

    const DirectX::TexMetadata imageMeta = image.GetMetadata();
    if (blockingDispatch(static_cast<UINT>(imageMeta.width), static_cast<UINT>(imageMeta.height), 1)
        != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Clean Up Objects
    inputTex.Reset();
    inputSRV.Reset();

    // Clean up shader resources
    array<ID3D11ShaderResourceView*, 2> nullSRV = { nullptr, nullptr };
    m_ptrContext->CSSetShaderResources(0, 1, nullSRV.data());
    array<ID3D11UnorderedAccessView*, 2> nullUAV = { nullptr, nullptr };
    m_ptrContext->CSSetUnorderedAccessViews(0, 1, nullUAV.data(), nullptr);
    m_ptrContext->CSSetShader(nullptr, nullptr, 0);

    // Read back data
    vector<array<int, 4>> data = readBack<array<int, 4>>(outputBuffer);

    // Cleanup
    outputBuffer.Reset();
    outputBufferUAV.Reset();

    m_ptrContext->Flush(); // Flush GPU to avoid leaks

    return data[0];
}

auto ParallaxGenD3D::checkIfAspectRatioMatches(
    const std::filesystem::path& ddsPath1, const std::filesystem::path& ddsPath2) -> bool
{

    ParallaxGenTask::PGResult pgResult {};

    // get metadata (should only pull headers, which is much faster)
    DirectX::TexMetadata ddsImageMeta1 {};
    pgResult = getDDSMetadata(ddsPath1, ddsImageMeta1);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return false;
    }

    DirectX::TexMetadata ddsImageMeta2 {};
    pgResult = getDDSMetadata(ddsPath2, ddsImageMeta2);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return false;
    }

    // calculate aspect ratios
    const float aspectRatio1 = static_cast<float>(ddsImageMeta1.width) / static_cast<float>(ddsImageMeta1.height);
    const float aspectRatio2 = static_cast<float>(ddsImageMeta2.width) / static_cast<float>(ddsImageMeta2.height);

    // check if aspect ratios don't match
    return aspectRatio1 == aspectRatio2;
}

//
// GPU Code
//

void ParallaxGenD3D::initGPU()
{
// initialize GPU device and context
#ifdef _DEBUG
    UINT deviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
    const UINT deviceFlags = 0;
#endif

    HRESULT hr {};
    hr = D3D11CreateDevice(nullptr, // Adapter (configured to use default)
        D3D_DRIVER_TYPE_HARDWARE, // User Hardware
        nullptr, // Irrelevant for hardware driver type
        deviceFlags, // Device flags (enables debug if needed)
        &s_featureLevel, // Feature levels this app can support
        1, // Just 1 feature level (D3D11)
        D3D11_SDK_VERSION, // Set to D3D11 SDK version
        &m_ptrDevice, // Sets the instance device
        nullptr, // Returns feature level of device created (not needed)
        &m_ptrContext // Sets the instance device immediate context
    );

    // check if device was found successfully
    if (FAILED(hr)) {
        spdlog::error("D3D11 device creation failure error: {}", getHRESULTErrorMessage(hr));
        spdlog::critical("Unable to find any DX11 capable devices. Disable any GPU-accelerated "
                         "features to continue.");
        exit(1);
    }

    // Init Shaders
    initShaders();
}

void ParallaxGenD3D::initShaders()
{
    ParallaxGenTask::PGResult pgResult {};

    // TODO don't repeat error handling code
    // ConvertToHDR.hlsl
    pgResult = createComputeShader(L"ConvertToHDR.hlsl", m_shaderConvertToHDR);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        exit(1);
    }

    // MergeToComplexMaterial.hlsl
    pgResult = createComputeShader(L"MergeToComplexMaterial.hlsl", m_shaderMergeToComplexMaterial);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        exit(1);
    }

    // CountAlphaValues.hlsl
    pgResult = createComputeShader(L"CountAlphaValues.hlsl", m_shaderCountAlphaValues);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        exit(1);
    }
}

auto ParallaxGenD3D::compileShader(const std::filesystem::path& filename, ComPtr<ID3DBlob>& shaderBlob) const
    -> ParallaxGenTask::PGResult
{
    spdlog::trace(L"Starting compiling shader: {}", filename.wstring());

#if defined(_DEBUG)
    const DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;
#else
    const DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#endif

    const filesystem::path shaderAbsPath = m_exePath / "shaders" / filename;

    ComPtr<ID3DBlob> ptrErrorBlob;
    const HRESULT hr = D3DCompileFromFile(shaderAbsPath.c_str(), nullptr, nullptr, "main", "cs_5_0", dwShaderFlags, 0,
        shaderBlob.ReleaseAndGetAddressOf(), ptrErrorBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        if (ptrErrorBlob != nullptr) {
            spdlog::critical(L"Failed to compile shader {}: {}, {}", filename.wstring(),
                asciitoUTF16(getHRESULTErrorMessage(hr)), static_cast<wchar_t*>(ptrErrorBlob->GetBufferPointer()));
            ptrErrorBlob.Reset();
        } else {
            spdlog::critical(
                L"Failed to compile shader {}: {}", filename.wstring(), asciitoUTF16(getHRESULTErrorMessage(hr)));
        }

        exit(1);
    }

    spdlog::debug(L"Shader {} compiled successfully", filename.wstring());

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createComputeShader(const wstring& shaderPath, ComPtr<ID3D11ComputeShader>& shaderDest)
    -> ParallaxGenTask::PGResult
{
    // Load shader
    ComPtr<ID3DBlob> compiledShader;
    compileShader(shaderPath, compiledShader);

    // Create shader
    const HRESULT hr = m_ptrDevice->CreateComputeShader(compiledShader->GetBufferPointer(),
        compiledShader->GetBufferSize(), nullptr, shaderDest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create compute shader: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::upgradeToComplexMaterial(
    const std::filesystem::path& parallaxMap, const std::filesystem::path& envMap) -> DirectX::ScratchImage
{
    if ((m_ptrContext == nullptr) || (m_ptrDevice == nullptr) || (m_shaderMergeToComplexMaterial == nullptr)) {
        throw runtime_error("GPU was not initialized");
    }

    const lock_guard<mutex> lock(m_gpuOperationMutex);

    ParallaxGenTask::PGResult pgResult {};

    const bool parallaxExists = !parallaxMap.empty();
    const bool envExists = !envMap.empty();

    // Check if any texture was supplied
    if (!parallaxExists && !envExists) {
        return {};
    }

    if (envExists && m_pgd->getTextureType(envMap) == NIFUtil::TextureType::COMPLEXMATERIAL) {
        spdlog::trace(L"Skipping shader upgrade for {} as it is already a complex material", envMap.wstring());
        return {};
    }

    // get parallax map
    DirectX::ScratchImage parallaxMapDDS;
    if (parallaxExists && getDDS(parallaxMap, parallaxMapDDS) != ParallaxGenTask::PGResult::SUCCESS) {
        spdlog::debug(L"Failed to load DDS file: {}", parallaxMap.wstring());
        return {};
    }
    const DirectX::TexMetadata parallaxMeta = parallaxMapDDS.GetMetadata();

    // get env map
    DirectX::ScratchImage envMapDDS;
    if (envExists && getDDS(envMap, envMapDDS) != ParallaxGenTask::PGResult::SUCCESS) {
        spdlog::debug(L"Failed to load DDS file: {}", parallaxMap.wstring());
        return {};
    }
    const DirectX::TexMetadata envMeta = envMapDDS.GetMetadata();

    // Check dimensions
    size_t parallaxHeight = 0;
    size_t parallaxWidth = 0;
    float parallaxAspectRatio = 0.0F;
    if (parallaxExists) {
        parallaxHeight = parallaxMeta.height;
        parallaxWidth = parallaxMeta.width;
        parallaxAspectRatio = static_cast<float>(parallaxWidth) / static_cast<float>(parallaxHeight);
    }

    size_t envHeight = 0;
    size_t envWidth = 0;
    float envAspectRatio = 0.0F;
    if (envExists) {
        envHeight = envMeta.height;
        envWidth = envMeta.width;
        envAspectRatio = static_cast<float>(envWidth) / static_cast<float>(envHeight);
    }

    // Compare aspect ratios if both exist
    if (parallaxExists && envExists && parallaxAspectRatio != envAspectRatio) {
        spdlog::trace(L"Rejecting shader upgrade for {} due to aspect ratio mismatch with {}", parallaxMap.wstring(),
            envMap.wstring());
        return {};
    }

    // Create GPU Textures objects
    ComPtr<ID3D11Texture2D> parallaxMapGPU;
    ComPtr<ID3D11ShaderResourceView> parallaxMapSRV;
    if (parallaxExists) {
        pgResult = createTexture2D(parallaxMapDDS, parallaxMapGPU);
        if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
            spdlog::debug(L"Failed to create GPU texture for {}", parallaxMap.wstring());
            return {};
        }
        pgResult = createShaderResourceView(parallaxMapGPU, parallaxMapSRV);
        if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
            spdlog::debug(L"Failed to create GPU SRV for {}", parallaxMap.wstring());
            return {};
        }
    }
    ComPtr<ID3D11Texture2D> envMapGPU;
    ComPtr<ID3D11ShaderResourceView> envMapSRV;
    if (envExists) {
        pgResult = createTexture2D(envMapDDS, envMapGPU);
        if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
            return {};
        }
        pgResult = createShaderResourceView(envMapGPU, envMapSRV);
        if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
            return {};
        }
    }

    // Define parameters in constant buffer
    const UINT resultWidth = static_cast<UINT>(max(parallaxWidth, envWidth));
    const UINT resultHeight = static_cast<UINT>(max(parallaxHeight, envHeight));

    // calculate scaling weights
    float parallaxScalingWidth = 1.0F;
    float parallaxScalingHeight = 1.0F;
    float envScalingWidth = 1.0F;
    float envScalingHeight = 1.0F;

    if (envExists && parallaxExists) {
        parallaxScalingWidth = static_cast<float>(parallaxWidth) / static_cast<float>(resultWidth);
        parallaxScalingHeight = static_cast<float>(parallaxHeight) / static_cast<float>(resultHeight);
        envScalingWidth = static_cast<float>(envWidth) / static_cast<float>(resultWidth);
        envScalingHeight = static_cast<float>(envHeight) / static_cast<float>(resultHeight);
    }

    ShaderMergeToComplexMaterialParams shaderParams {};
    shaderParams.envMapScalingFactor = DirectX::XMFLOAT2(envScalingWidth, envScalingHeight);
    shaderParams.parallaxMapScalingFactor = DirectX::XMFLOAT2(parallaxScalingWidth, parallaxScalingHeight);
    shaderParams.envMapAvailable = static_cast<BOOL>(envExists);
    shaderParams.parallaxMapAvailable = static_cast<BOOL>(parallaxExists);
    shaderParams.intScalingFactor = MAX_CHANNEL_VALUE;

    ComPtr<ID3D11Buffer> constantBuffer;
    pgResult = createConstantBuffer(&shaderParams, sizeof(ShaderMergeToComplexMaterialParams), constantBuffer);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Create output texture
    D3D11_TEXTURE2D_DESC outputTextureDesc = {};
    if (parallaxExists) {
        parallaxMapGPU->GetDesc(&outputTextureDesc);
    } else {
        envMapGPU->GetDesc(&outputTextureDesc);
    }
    outputTextureDesc.Width = resultWidth;
    outputTextureDesc.Height = resultHeight;
    outputTextureDesc.MipLevels = 0;
    outputTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    outputTextureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> outputTexture;
    pgResult = createTexture2D(outputTextureDesc, outputTexture);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }
    // Query the texture's description to get the actual number of mip levels
    D3D11_TEXTURE2D_DESC createdOutputTextureDesc = {};
    outputTexture->GetDesc(&createdOutputTextureDesc);
    const UINT resultMips = createdOutputTextureDesc.MipLevels;

    ComPtr<ID3D11UnorderedAccessView> outputUAV;
    pgResult = createUnorderedAccessView(outputTexture, outputUAV);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Create buffer for output
    ShaderMergeToComplexMaterialOutputBuffer minMaxInitData
        = { .minEnvValue = UINT_MAX, .maxEnvValue = 0, .minParallaxValue = UINT_MAX, .maxParallaxValue = 0 };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(ShaderMergeToComplexMaterialOutputBuffer);
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(UINT);

    ComPtr<ID3D11Buffer> outputBuffer;
    pgResult = createBuffer(&minMaxInitData, bufferDesc, outputBuffer);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Create UAV for output buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 4;

    ComPtr<ID3D11UnorderedAccessView> outputBufferUAV;
    pgResult = createUnorderedAccessView(outputBuffer, uavDesc, outputBufferUAV);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Dispatch shader
    m_ptrContext->CSSetShader(m_shaderMergeToComplexMaterial.Get(), nullptr, 0);
    m_ptrContext->CSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    m_ptrContext->CSSetShaderResources(0, 1, envMapSRV.GetAddressOf());
    m_ptrContext->CSSetShaderResources(1, 1, parallaxMapSRV.GetAddressOf());
    m_ptrContext->CSSetUnorderedAccessViews(0, 1, outputUAV.GetAddressOf(), nullptr);
    m_ptrContext->CSSetUnorderedAccessViews(1, 1, outputBufferUAV.GetAddressOf(), nullptr);

    pgResult = blockingDispatch(resultWidth, resultHeight, 1);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Clean up shader resources
    array<ID3D11ShaderResourceView*, 2> nullSRV = { nullptr, nullptr };
    m_ptrContext->CSSetShaderResources(0, 2, nullSRV.data());
    array<ID3D11UnorderedAccessView*, 2> nullUAV = { nullptr, nullptr };
    m_ptrContext->CSSetUnorderedAccessViews(0, 2, nullUAV.data(), nullptr);
    array<ID3D11Buffer*, 1> nullBuffer = { nullptr };
    m_ptrContext->CSSetConstantBuffers(0, 1, nullBuffer.data());
    m_ptrContext->CSSetShader(nullptr, nullptr, 0);

    // Release some objects
    parallaxMapGPU.Reset();
    envMapGPU.Reset();
    parallaxMapSRV.Reset();
    envMapSRV.Reset();
    constantBuffer.Reset();

    // Generate mips
    D3D11_TEXTURE2D_DESC outputTextureMipsDesc = {};
    outputTexture->GetDesc(&outputTextureMipsDesc);
    outputTextureMipsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    outputTextureMipsDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ComPtr<ID3D11Texture2D> outputTextureMips;
    pgResult = createTexture2D(outputTextureMipsDesc, outputTextureMips);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }
    ComPtr<ID3D11ShaderResourceView> outputMipsSRV;
    pgResult = createShaderResourceView(outputTextureMips, outputMipsSRV);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        return {};
    }

    // Copy texture
    m_ptrContext->CopyResource(outputTextureMips.Get(), outputTexture.Get());
    m_ptrContext->GenerateMips(outputMipsSRV.Get());
    m_ptrContext->CopyResource(outputTexture.Get(), outputTextureMips.Get());

    // cleanup
    outputTextureMips.Reset();
    outputMipsSRV.Reset();

    // Read back output buffer
    auto outputBufferData = readBack<UINT>(outputBuffer);

    // Read back texture (DEBUG)
    auto outputTextureData = readBack(outputTexture);

    // More cleaning
    outputTexture.Reset();
    outputUAV.Reset();
    outputBuffer.Reset();
    outputBufferUAV.Reset();

    // Flush GPU to avoid leaks
    m_ptrContext->Flush();

    // Import into directx scratchimage
    const DirectX::ScratchImage outputImage = loadRawPixelsToScratchImage(
        outputTextureData, resultWidth, resultHeight, resultMips, DXGI_FORMAT_R8G8B8A8_UNORM);

    // Compress DDS
    // BC3 works best with heightmaps
    DirectX::ScratchImage compressedImage;
    const HRESULT hr = DirectX::Compress(outputImage.GetImages(), outputImage.GetImageCount(),
        outputImage.GetMetadata(), DXGI_FORMAT_BC3_UNORM, DirectX::TEX_COMPRESS_DEFAULT, 1.0F, compressedImage);
    if (FAILED(hr)) {
        spdlog::debug("Failed to compress output DDS file: {}", getHRESULTErrorMessage(hr));
        return {};
    }

    return compressedImage;
}

void ParallaxGenD3D::convertToHDR(
    DirectX::ScratchImage* dds, bool& ddsModified, const float& luminanceMult, const DXGI_FORMAT& outputFormat)
{
    if ((m_ptrContext == nullptr) || (m_ptrDevice == nullptr) || (m_shaderMergeToComplexMaterial == nullptr)) {
        throw runtime_error("GPU was not initialized");
    }

    if (dds == nullptr) {
        throw runtime_error("DDS image is null");
    }

    const lock_guard<mutex> lock(m_gpuOperationMutex);

    const DirectX::TexMetadata inputMeta = dds->GetMetadata();

    // Load texture on GPU
    ComPtr<ID3D11Texture2D> inputDDSGPU;
    if (createTexture2D(*dds, inputDDSGPU) != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    // Create shader resource view
    ComPtr<ID3D11ShaderResourceView> inputDDSSRV;
    if (createShaderResourceView(inputDDSGPU, inputDDSSRV) != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    // Create constant parameter buffer
    ShaderConvertToHDRParams shaderParams {};
    shaderParams.luminanceMult = luminanceMult;

    ComPtr<ID3D11Buffer> constantBuffer;
    if (createConstantBuffer(&shaderParams, sizeof(ShaderConvertToHDRParams), constantBuffer)
        != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    // Create output texture
    D3D11_TEXTURE2D_DESC outputDDSDesc = {};
    inputDDSGPU->GetDesc(&outputDDSDesc);
    outputDDSDesc.Format = outputFormat;
    outputDDSDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> outputDDSGPU;
    if (createTexture2D(outputDDSDesc, outputDDSGPU) != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    ComPtr<ID3D11UnorderedAccessView> outputDDSUAV;
    if (createUnorderedAccessView(outputDDSGPU, outputDDSUAV) != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    // Dispatch shader
    m_ptrContext->CSSetShader(m_shaderConvertToHDR.Get(), nullptr, 0);
    m_ptrContext->CSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    m_ptrContext->CSSetShaderResources(0, 1, inputDDSSRV.GetAddressOf());
    m_ptrContext->CSSetUnorderedAccessViews(0, 1, outputDDSUAV.GetAddressOf(), nullptr);

    if (blockingDispatch(static_cast<UINT>(inputMeta.width), static_cast<UINT>(inputMeta.height), 1)
        != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    // Clean up shader resources
    array<ID3D11Buffer*, 1> nullBuffer = { nullptr };
    m_ptrContext->CSSetConstantBuffers(0, 1, nullBuffer.data());
    array<ID3D11ShaderResourceView*, 1> nullSRV = { nullptr };
    m_ptrContext->CSSetShaderResources(0, 1, nullSRV.data());
    array<ID3D11UnorderedAccessView*, 1> nullUAV = { nullptr };
    m_ptrContext->CSSetUnorderedAccessViews(0, 1, nullUAV.data(), nullptr);
    m_ptrContext->CSSetShader(nullptr, nullptr, 0);

    // Release some objects
    inputDDSGPU.Reset();
    inputDDSSRV.Reset();
    constantBuffer.Reset();

    // Generate mips
    D3D11_TEXTURE2D_DESC outputDDSMipsDesc = {};
    outputDDSGPU->GetDesc(&outputDDSMipsDesc);
    outputDDSMipsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    outputDDSMipsDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ComPtr<ID3D11Texture2D> outputDDSMipsGPU;
    if (createTexture2D(outputDDSMipsDesc, outputDDSMipsGPU) != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    ComPtr<ID3D11ShaderResourceView> outputMipsSRV;
    if (createShaderResourceView(outputDDSMipsGPU, outputMipsSRV) != ParallaxGenTask::PGResult::SUCCESS) {
        return;
    }

    // Copy texture
    m_ptrContext->CopyResource(outputDDSMipsGPU.Get(), outputDDSGPU.Get());
    m_ptrContext->GenerateMips(outputMipsSRV.Get());
    m_ptrContext->CopyResource(outputDDSGPU.Get(), outputDDSMipsGPU.Get());

    // cleanup
    outputDDSMipsGPU.Reset();
    outputMipsSRV.Reset();

    // Read back texture
    auto outputTextureData = readBack(outputDDSGPU);

    // More cleaning
    outputDDSGPU.Reset();
    outputDDSUAV.Reset();

    // Flush GPU to avoid leaks
    m_ptrContext->Flush();

    // Import into directx scratchimage
    *dds = loadRawPixelsToScratchImage(
        outputTextureData, inputMeta.width, inputMeta.height, inputMeta.mipLevels, outputFormat);
    ddsModified = true;
}

//
// GPU Helpers
//
auto ParallaxGenD3D::isPowerOfTwo(unsigned int x) -> bool { return (x != 0U) && ((x & (x - 1)) == 0U); }

auto ParallaxGenD3D::createTexture2D(const DirectX::ScratchImage& texture, ComPtr<ID3D11Texture2D>& dest) const
    -> ParallaxGenTask::PGResult
{
    // Define error object
    HRESULT hr {};

    // Verify dimention
    auto textureMeta = texture.GetMetadata();
    if (!isPowerOfTwo(static_cast<unsigned int>(textureMeta.width))
        || !isPowerOfTwo(static_cast<unsigned int>(textureMeta.height))) {
        spdlog::debug("Texture dimensions must be a power of 2: {}x{}", textureMeta.width, textureMeta.height);
        return ParallaxGenTask::PGResult::FAILURE;
    }

    // Create texture
    hr = DirectX::CreateTexture(m_ptrDevice.Get(), texture.GetImages(), texture.GetImageCount(), texture.GetMetadata(),
        reinterpret_cast<ID3D11Resource**>(dest.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createTexture2D(ComPtr<ID3D11Texture2D>& existingTexture, ComPtr<ID3D11Texture2D>& dest) const
    -> ParallaxGenTask::PGResult
{
    // Smart Pointer to hold texture for output
    D3D11_TEXTURE2D_DESC textureOutDesc;
    existingTexture->GetDesc(&textureOutDesc);

    HRESULT hr {};

    // Create texture
    hr = m_ptrDevice->CreateTexture2D(&textureOutDesc, nullptr, dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    // Create texture from description
    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createTexture2D(D3D11_TEXTURE2D_DESC& desc, ComPtr<ID3D11Texture2D>& dest) const
    -> ParallaxGenTask::PGResult
{
    // Define error object
    HRESULT hr {};

    // Create texture
    hr = m_ptrDevice->CreateTexture2D(&desc, nullptr, dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createShaderResourceView(
    const ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11ShaderResourceView>& dest) const -> ParallaxGenTask::PGResult
{
    // Define error object
    HRESULT hr {};

    // Create SRV for texture
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc = {};
    shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderDesc.Texture2D.MipLevels = -1;

    // Create SRV
    hr = m_ptrDevice->CreateShaderResourceView(texture.Get(), &shaderDesc, dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11ShaderResourceView on GPU: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createUnorderedAccessView(
    const ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11UnorderedAccessView>& dest) const -> ParallaxGenTask::PGResult
{
    // Define error object
    HRESULT hr {};

    // Create UAV for texture
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    // Create UAV
    hr = m_ptrDevice->CreateUnorderedAccessView(texture.Get(), &uavDesc, dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11UnorderedAccessView on GPU: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createUnorderedAccessView(const ComPtr<ID3D11Resource>& gpuResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc, ComPtr<ID3D11UnorderedAccessView>& dest) const
    -> ParallaxGenTask::PGResult
{
    HRESULT hr {};

    hr = m_ptrDevice->CreateUnorderedAccessView(gpuResource.Get(), &desc, &dest);
    if (FAILED(hr)) {
        spdlog::debug("Failed to create ID3D11UnorderedAccessView on GPU: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createBuffer(const void* data, D3D11_BUFFER_DESC& desc, ComPtr<ID3D11Buffer>& dest) const
    -> ParallaxGenTask::PGResult
{
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    const HRESULT hr = m_ptrDevice->CreateBuffer(&desc, &initData, dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create ID3D11Buffer on GPU: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createConstantBuffer(const void* data, const UINT& size, ComPtr<ID3D11Buffer>& dest) const
    -> ParallaxGenTask::PGResult
{
    // Define error object
    HRESULT hr {};

    // Create buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = (size + GPU_BUFFER_SIZE_MULTIPLE) - (size % GPU_BUFFER_SIZE_MULTIPLE);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    // Create init data
    D3D11_SUBRESOURCE_DATA cbInitData;
    cbInitData.pSysMem = data;
    cbInitData.SysMemPitch = 0;
    cbInitData.SysMemSlicePitch = 0;

    // Create buffer
    hr = m_ptrDevice->CreateBuffer(&cbDesc, &cbInitData, dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Buffer on GPU: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::blockingDispatch(UINT threadGroupCountX, UINT threadGroupCountY, UINT threadGroupCountZ) const
    -> ParallaxGenTask::PGResult
{
    // Error object
    HRESULT hr {};

    // query
    ComPtr<ID3D11Query> ptrQuery = nullptr;
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_EVENT;
    hr = m_ptrDevice->CreateQuery(&queryDesc, ptrQuery.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create query: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    // Run the shader
    m_ptrContext->Dispatch((threadGroupCountX + NUM_GPU_THREADS - 1) / NUM_GPU_THREADS,
        (threadGroupCountY + NUM_GPU_THREADS - 1) / NUM_GPU_THREADS,
        (threadGroupCountZ + NUM_GPU_THREADS - 1) / NUM_GPU_THREADS);

    // end query
    m_ptrContext->End(ptrQuery.Get());

    // wait for shader to complete
    BOOL queryData = FALSE;
    hr = m_ptrContext->GetData(ptrQuery.Get(), &queryData, sizeof(queryData),
        D3D11_ASYNC_GETDATA_DONOTFLUSH); // block until complete
    if (FAILED(hr)) {
        spdlog::debug("Failed to get query data: {}", getHRESULTErrorMessage(hr));
        return ParallaxGenTask::PGResult::FAILURE;
    }
    ptrQuery.Reset();

    // return success
    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::readBack(const ComPtr<ID3D11Texture2D>& gpuResource) const -> vector<unsigned char>
{
    // Error object
    HRESULT hr {};
    ParallaxGenTask::PGResult pgResult {};

    // Grab texture description
    D3D11_TEXTURE2D_DESC stagingTex2DDesc;
    gpuResource->GetDesc(&stagingTex2DDesc);
    const UINT mipLevels = stagingTex2DDesc.MipLevels; // Number of mip levels to read back

    // Enable flags for CPU access
    stagingTex2DDesc.Usage = D3D11_USAGE_STAGING;
    stagingTex2DDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingTex2DDesc.BindFlags = 0;
    stagingTex2DDesc.MiscFlags = 0;

    // Find bytes per pixel based on format
    UINT bytesPerChannel = 1; // Default to 8-bit
    UINT numChannels = 4; // Default to RGBA
    switch (stagingTex2DDesc.Format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        bytesPerChannel = 4; // 32 bits per channel
        numChannels = 4; // RGBA
        break;
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        bytesPerChannel = 4; // 32 bits per channel
        numChannels = 3; // RGB
        break;
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
        bytesPerChannel = 4; // 32 bits per channel
        numChannels = 2; // RG
        break;
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
        bytesPerChannel = 4; // 32 bits per channel
        numChannels = 1; // R
        break;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
        bytesPerChannel = 2; // 16 bits per channel
        numChannels = 4; // RGBA
        break;
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
        bytesPerChannel = 2; // 16 bits per channel
        numChannels = 2; // RG
        break;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
        bytesPerChannel = 2; // 16 bits per channel
        numChannels = 1; // R
        break;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
        bytesPerChannel = 1; // 8 bits per channel
        numChannels = 4; // RGBA
        break;
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
        bytesPerChannel = 1; // 8 bits per channel
        numChannels = 2; // RG
        break;
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
        bytesPerChannel = 1; // 8 bits per channel
        numChannels = 1; // R
        break;
    // Add more cases for other formats if needed
    default:
        bytesPerChannel = 1; // Assume 8-bit for unsupported formats
        numChannels = 4; // Assume RGBA for unsupported formats
    }
    const UINT bytesPerPixel = bytesPerChannel * numChannels;

    // Create staging texture
    ComPtr<ID3D11Texture2D> stagingTex2D;
    pgResult = createTexture2D(stagingTex2DDesc, stagingTex2D);
    if (pgResult != ParallaxGenTask::PGResult::SUCCESS) {
        spdlog::debug("Failed to create staging texture: {}", getHRESULTErrorMessage(hr));
        return {};
    }

    // Copy resource to staging texture
    m_ptrContext->CopyResource(stagingTex2D.Get(), gpuResource.Get());

    std::vector<unsigned char> outputData;

    // Iterate over each mip level
    for (UINT mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
        // Get dimensions for the current mip level
        const UINT mipWidth = std::max(1U, stagingTex2DDesc.Width >> mipLevel);
        const UINT mipHeight = std::max(1U, stagingTex2DDesc.Height >> mipLevel);

        // Map the mip level to the CPU
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = m_ptrContext->Map(stagingTex2D.Get(), mipLevel, D3D11_MAP_READ, 0, &mappedResource);
        if (FAILED(hr)) {
            spdlog::debug("[GPU] Failed to map resource to CPU during read back at mip level {}: {}", mipLevel,
                getHRESULTErrorMessage(hr));
            return {};
        }

        // Copy the data from the mapped resource to the output vector
        auto* srcData = reinterpret_cast<unsigned char*>(mappedResource.pData);
        for (UINT row = 0; row < mipHeight; ++row) {
            unsigned char* rowStart = srcData + (row * mappedResource.RowPitch);
            outputData.insert(outputData.end(), rowStart, rowStart + (mipWidth * bytesPerPixel));
        }

        // Unmap the resource for this mip level
        m_ptrContext->Unmap(stagingTex2D.Get(), mipLevel);
    }

    stagingTex2D.Reset();

    return outputData;
}

template <typename T> auto ParallaxGenD3D::readBack(const ComPtr<ID3D11Buffer>& gpuResource) const -> vector<T>
{
    // Error object
    HRESULT hr {};

    // grab edge detection results
    D3D11_BUFFER_DESC bufferDesc;
    gpuResource->GetDesc(&bufferDesc);
    // Enable flags for CPU access
    bufferDesc.Usage = D3D11_USAGE_STAGING;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bufferDesc.BindFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    // Create the staging buffer
    ComPtr<ID3D11Buffer> stagingBuffer;
    hr = m_ptrDevice->CreateBuffer(&bufferDesc, nullptr, stagingBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("[GPU] Failed to create staging buffer: {}", getHRESULTErrorMessage(hr));
        return std::vector<T>();
    }

    // copy resource
    m_ptrContext->CopyResource(stagingBuffer.Get(), gpuResource.Get());

    // map resource to CPU
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_ptrContext->Map(stagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        spdlog::debug("[GPU] Failed to map resource to CPU during read back: {}", getHRESULTErrorMessage(hr));
        return vector<T>();
    }

    // Access the texture data from MappedResource.pData
    const size_t numElements = bufferDesc.ByteWidth / sizeof(T);
    std::vector<T> outputData(
        reinterpret_cast<T*>(mappedResource.pData), reinterpret_cast<T*>(mappedResource.pData) + numElements);

    // Cleaup
    m_ptrContext->Unmap(stagingBuffer.Get(), 0); // cleanup map

    stagingBuffer.Reset();

    return outputData;
}

//
// Texture Helpers
//

auto ParallaxGenD3D::getDDS(const filesystem::path& ddsPath, DirectX::ScratchImage& dds) const
    -> ParallaxGenTask::PGResult
{
    HRESULT hr {};

    if (m_pgd->isLooseFile(ddsPath)) {
        spdlog::trace(L"Reading DDS loose file {}", ddsPath.wstring());
        const filesystem::path fullPath = m_pgd->getLooseFileFullPath(ddsPath);

        // Load DDS file
        hr = DirectX::LoadFromDDSFile(fullPath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, dds);
    } else if (m_pgd->isBSAFile(ddsPath)) {
        spdlog::trace(L"Reading DDS BSA file {}", ddsPath.wstring());
        vector<std::byte> ddsBytes = m_pgd->getFile(ddsPath);

        // Load DDS file
        hr = DirectX::LoadFromDDSMemory(ddsBytes.data(), ddsBytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, dds);
    } else {
        spdlog::trace(L"Reading DDS file from output dir {}", ddsPath.wstring());
        const filesystem::path fullPath = m_outputDir / ddsPath;

        // Load DDS file
        hr = DirectX::LoadFromDDSFile(fullPath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, dds);
    }

    if (FAILED(hr)) {
        spdlog::debug(
            L"Failed to load DDS file from {}: {}", ddsPath.wstring(), asciitoUTF16(getHRESULTErrorMessage(hr)));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::getDDSMetadata(const filesystem::path& ddsPath, DirectX::TexMetadata& ddsMeta)
    -> ParallaxGenTask::PGResult
{
    // Use lock_guard to make this method thread-safe
    const lock_guard<mutex> lock(m_ddsMetaDataMutex);

    // Check if in cache
    // TODO set cache to something on failure
    if (m_ddsMetaDataCache.find(ddsPath) != m_ddsMetaDataCache.end()) {
        ddsMeta = m_ddsMetaDataCache[ddsPath];
        return ParallaxGenTask::PGResult::SUCCESS;
    }

    HRESULT hr {};

    if (m_pgd->isLooseFile(ddsPath)) {
        spdlog::trace(L"Reading DDS loose file metadata {}", ddsPath.wstring());
        const filesystem::path fullPath = m_pgd->getLooseFileFullPath(ddsPath);

        // Load DDS file
        hr = DirectX::GetMetadataFromDDSFile(fullPath.c_str(), DirectX::DDS_FLAGS_NONE, ddsMeta);
    } else if (m_pgd->isBSAFile(ddsPath)) {
        spdlog::trace(L"Reading DDS BSA file metadata {}", ddsPath.wstring());
        vector<std::byte> ddsBytes = m_pgd->getFile(ddsPath);

        // Load DDS file
        hr = DirectX::GetMetadataFromDDSMemory(ddsBytes.data(), ddsBytes.size(), DirectX::DDS_FLAGS_NONE, ddsMeta);
    } else {
        spdlog::trace(L"Reading DDS file from output dir {}", ddsPath.wstring());
        const filesystem::path fullPath = m_outputDir / ddsPath;

        // Load DDS file
        hr = DirectX::GetMetadataFromDDSFile(fullPath.c_str(), DirectX::DDS_FLAGS_NONE, ddsMeta);
    }

    if (FAILED(hr)) {
        spdlog::debug(L"Failed to load DDS file metadata from {}: {}", ddsPath.wstring(),
            asciitoUTF16(getHRESULTErrorMessage(hr)));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    // update cache
    m_ddsMetaDataCache[ddsPath] = ddsMeta;

    return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::loadRawPixelsToScratchImage(const vector<unsigned char>& rawPixels, const size_t& width,
    const size_t& height, const size_t& mips, DXGI_FORMAT format) -> DirectX::ScratchImage
{
    // Initialize a ScratchImage
    DirectX::ScratchImage image;
    const HRESULT hr = image.Initialize2D(format, width, height, 1,
        mips); // 1 array slice, 1 mipmap level
    if (FAILED(hr)) {
        spdlog::debug("Failed to initialize ScratchImage: {}", getHRESULTErrorMessage(hr));
        return {};
    }

    // Get the image data
    const DirectX::Image* img = image.GetImage(0, 0, 0);
    if (img == nullptr) {
        spdlog::debug("Failed to get image data from ScratchImage");
        return {};
    }

    // Copy the raw pixel data into the image
    memcpy(img->pixels, rawPixels.data(), rawPixels.size());

    return image;
}

auto ParallaxGenD3D::getHRESULTErrorMessage(HRESULT hr) -> string
{
    // Get error message
    const _com_error err(hr);
    return err.ErrorMessage();
}

auto ParallaxGenD3D::getDXGIFormatFromString(const string& format) -> DXGI_FORMAT
{
    if (format == "rgba16f") {
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    }

    if (format == "rgba32f") {
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }

    return DXGI_FORMAT_UNKNOWN;
}

// NOLINTEND(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
