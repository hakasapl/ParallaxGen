#include "ParallaxGenD3D/ParallaxGenD3D.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <d3dcompiler.h>
#include <comdef.h>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using Microsoft::WRL::ComPtr;

ParallaxGenD3D::ParallaxGenD3D(ParallaxGenDirectory* pgd, std::filesystem::path output_dir)
{
    // Constructor
    this->pgd = pgd;

    // Set output directory
    this->output_dir = output_dir;
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
// GPU Code
//

void ParallaxGenD3D::initGPU()
{
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
        spdlog::error("D3D11 device creation failure error: {}", getHRESULTErrorMessage(hr));
        spdlog::critical("Unable to find any DX11 capable devices. Disable any GPU-accelerated features to continue.");
        ParallaxGenUtil::exitWithUserInput(1);
	}

    // Init Shaders
    initShaders();
}

void ParallaxGenD3D::initShaders()
{
    // MergeToComplexMaterial.hlsl
    if (!createComputeShader(L"shaders/MergeToComplexMaterial.hlsl", shader_MergeToComplexMaterial)) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
    }
}

ComPtr<ID3DBlob> ParallaxGenD3D::compileShader(const std::filesystem::path& filename) {
    spdlog::trace(L"Starting compiling shader: {}", filename.wstring());

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
    #if defined( DEBUG ) || defined( _DEBUG )
        dwShaderFlags |= D3DCOMPILE_DEBUG;
    #endif

    ComPtr<ID3DBlob> pCSBlob;
    ComPtr<ID3DBlob> pErrorBlob;
    HRESULT hr = D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "main", "cs_5_0", dwShaderFlags, 0, pCSBlob.ReleaseAndGetAddressOf(), pErrorBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        if (pErrorBlob) {
            spdlog::critical(L"Failed to compile shader {}: {}, {}", filename.wstring(), ParallaxGenUtil::convertToWstring(getHRESULTErrorMessage(hr)), static_cast<wchar_t*>(pErrorBlob->GetBufferPointer()));
            pErrorBlob.Reset();
        } else {
            spdlog::critical(L"Failed to compile shader {}: {}", filename.wstring(), ParallaxGenUtil::convertToWstring(getHRESULTErrorMessage(hr)));
        }

        ParallaxGenUtil::exitWithUserInput(1);
    }

    spdlog::debug(L"Shader {} compiled successfully", filename.wstring());

    return pCSBlob;
}

bool ParallaxGenD3D::createComputeShader(const wstring& shader_path, ComPtr<ID3D11ComputeShader>& cs_dest)
{
    // Load shader
    ComPtr<ID3DBlob> compiled_shader = compileShader(shader_path);

    // Create shader
    HRESULT hr = pDevice->CreateComputeShader(compiled_shader->GetBufferPointer(), compiled_shader->GetBufferSize(), nullptr, cs_dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create compute shader: {}", getHRESULTErrorMessage(hr));
        return false;
    }

    return true;
}

DirectX::ScratchImage ParallaxGenD3D::upgradeToComplexMaterial(const std::filesystem::path& parallax_map, const std::filesystem::path& env_map) const
{
    bool parallax_exists = !parallax_map.empty();
    bool env_exists = !env_map.empty();

    // Check if any texture was supplied
    if (!parallax_exists && !env_exists) {
        spdlog::warn(L"Unable to upgrade to complex material, no textures supplied. {} and {}", parallax_map.wstring(), env_map.wstring());
        return DirectX::ScratchImage();
    }

    // get parallax map
    DirectX::ScratchImage parallax_map_dds;
    if (parallax_exists && !getDDS(parallax_map, parallax_map_dds)) {
        spdlog::warn(L"Failed to load DDS file: {} (skipping)", parallax_map.wstring());
        return DirectX::ScratchImage();
    }
    DirectX::TexMetadata parallax_meta = parallax_map_dds.GetMetadata();

    // get env map
    DirectX::ScratchImage env_map_dds;
    if (env_exists && !getDDS(env_map, env_map_dds)) {
        spdlog::warn(L"Failed to load DDS file: {} (skipping)", parallax_map.wstring());
        return DirectX::ScratchImage();
    }
    DirectX::TexMetadata env_meta = env_map_dds.GetMetadata();

    // Check dimensions
    size_t parallax_height = 0;
    size_t parallax_width = 0;
    float parallax_aspect_ratio;
    if (parallax_exists) {
        parallax_height = parallax_meta.height;
        parallax_width = parallax_meta.width;
        parallax_aspect_ratio = static_cast<float>(parallax_width) / static_cast<float>(parallax_height);
    }

    size_t env_height = 0;
    size_t env_width = 0;
    float env_aspect_ratio;
    if (env_exists) {
        env_height = env_meta.height;
        env_width = env_meta.width;
        env_aspect_ratio = static_cast<float>(env_width) / static_cast<float>(env_height);
    }

    // Compare aspect ratios if both exist
    if (parallax_exists && env_exists && parallax_aspect_ratio != env_aspect_ratio) {
        spdlog::trace(L"Rejecting shader upgrade for {} due to aspect ratio mismatch with {}", parallax_map.wstring(), env_map.wstring());
        return DirectX::ScratchImage();
    }

    // Create GPU Textures objects
    ComPtr<ID3D11Texture2D> parallax_map_gpu;
    ComPtr<ID3D11ShaderResourceView> parallax_map_srv;
    if (parallax_exists) {
        parallax_map_gpu = createTexture2D(parallax_map_dds);
        if (!parallax_map_gpu) {
            spdlog::warn(L"Failed to create GPU texture for {}", parallax_map.wstring());
            return DirectX::ScratchImage();
        }
        parallax_map_srv = createShaderResourceView(parallax_map_gpu);
        if (!parallax_map_srv) {
            spdlog::warn(L"Failed to create GPU SRV for {}", parallax_map.wstring());
            return DirectX::ScratchImage();
        }
    }
    ComPtr<ID3D11Texture2D> env_map_gpu;
    ComPtr<ID3D11ShaderResourceView> env_map_srv;
    if (env_exists) {
        env_map_gpu = createTexture2D(env_map_dds);
        if (!env_map_gpu) {
            return DirectX::ScratchImage();
        }
        env_map_srv = createShaderResourceView(env_map_gpu);
        if (!env_map_srv) {
            return DirectX::ScratchImage();
        }
    }

    // Define parameters in constant buffer
    UINT result_width = static_cast<UINT>(max(parallax_width, env_width));
    UINT result_height = static_cast<UINT>(max(parallax_height, env_height));

    // calculate scaling weights
    float parallax_scaling_width = 1.0f;
    float parallax_scaling_height = 1.0f;
    float env_scaling_width = 1.0f;
    float env_scaling_height = 1.0f;

    if (env_exists && parallax_exists) {
        parallax_scaling_width =  static_cast<float>(parallax_width) / static_cast<float>(result_width);
        parallax_scaling_height =  static_cast<float>(parallax_height) / static_cast<float>(result_height);
        env_scaling_width =  static_cast<float>(env_width) / static_cast<float>(result_width);
        env_scaling_height =  static_cast<float>(env_height) / static_cast<float>(result_height);
    }

    shader_MergeToComplexMaterial_params shader_params;
    shader_params.EnvMapScalingFactor = DirectX::XMFLOAT2(env_scaling_width, env_scaling_height);
    shader_params.ParallaxMapScalingFactor = DirectX::XMFLOAT2(parallax_scaling_width, parallax_scaling_height);
    shader_params.EnvMapAvailable = env_exists;
    shader_params.ParallaxMapAvailable = parallax_exists;
    shader_params.intScalingFactor = 255;

    ComPtr<ID3D11Buffer> constant_buffer = createConstantBuffer(&shader_params, sizeof(shader_MergeToComplexMaterial_params));
    if (!constant_buffer) {
        return DirectX::ScratchImage();
    }

    // Create output texture
    D3D11_TEXTURE2D_DESC output_texture_desc = {};
    if (parallax_exists) {
        parallax_map_gpu->GetDesc(&output_texture_desc);
    } else {
        env_map_gpu->GetDesc(&output_texture_desc);
    }
    output_texture_desc.Width = result_width;
    output_texture_desc.Height = result_height;
    output_texture_desc.MipLevels = 1;
    output_texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    output_texture_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> output_texture = createTexture2D(output_texture_desc);
    if (!output_texture) {
        return DirectX::ScratchImage();
    }
    ComPtr<ID3D11UnorderedAccessView> output_uav = createUnorderedAccessView(output_texture);
    if (!output_uav) {
        return DirectX::ScratchImage();
    }

    // Create buffer for output
    shader_MergeToComplexMaterial_outputbuffer minMaxInitData = { UINT_MAX, 0, UINT_MAX, 0 };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(shader_MergeToComplexMaterial_outputbuffer);
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(UINT);

    ComPtr<ID3D11Buffer> output_buffer = createBuffer(&minMaxInitData, bufferDesc);
    if (!output_buffer) {
        return DirectX::ScratchImage();
    }

    // Create UAV for output buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 4; // Number of UINT elements in the buffer

    ComPtr<ID3D11UnorderedAccessView> output_buffer_uav = createUnorderedAccessView(output_buffer, uavDesc);
    if (!output_buffer_uav) {
        return DirectX::ScratchImage();
    }

    // Dispatch shader
    pContext->CSSetShader(shader_MergeToComplexMaterial.Get(), nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
    pContext->CSSetShaderResources(0, 1, env_map_srv.GetAddressOf());
    pContext->CSSetShaderResources(1, 1, parallax_map_srv.GetAddressOf());
    pContext->CSSetUnorderedAccessViews(0, 1, output_uav.GetAddressOf(), nullptr);
    pContext->CSSetUnorderedAccessViews(1, 1, output_buffer_uav.GetAddressOf(), nullptr);

    bool result_bool = BlockingDispatch(result_width, result_height, 1);
    if (!result_bool) {
        return DirectX::ScratchImage();
    }

    // Clean up shader resources
    ID3D11ShaderResourceView* null_srv[] = { nullptr, nullptr };
    pContext->CSSetShaderResources(0, 2, null_srv);
    ID3D11UnorderedAccessView* null_uav[] = { nullptr, nullptr };
    pContext->CSSetUnorderedAccessViews(0, 2, null_uav, nullptr);
    ID3D11Buffer* null_buffer[] = { nullptr };
    pContext->CSSetConstantBuffers(0, 1, null_buffer);
    ID3D11ComputeShader* null_shader[] = { nullptr };
    pContext->CSSetShader(null_shader[0], nullptr, 0);

    // Release some objects
    parallax_map_gpu.Reset();
    env_map_gpu.Reset();
    parallax_map_srv.Reset();
    env_map_srv.Reset();
    constant_buffer.Reset();

    // Read back output buffer
    auto output_buffer_data = readBack<UINT>(output_buffer);

    // Read back texture (DEBUG)
    auto output_texture_data = readBack(output_texture, 4);

    // More cleaning
    output_texture.Reset();
    output_uav.Reset();
    output_buffer.Reset();
    output_buffer_uav.Reset();

    // Flush GPU to avoid leaks
    pContext->Flush();

    // Import into directx scratchimage
    DirectX::ScratchImage output_image = LoadRawPixelsToScratchImage(output_texture_data, result_width, result_height, DXGI_FORMAT_R8G8B8A8_UNORM, 4);

    // Compress dds
    // BC3 works best with heightmaps
    DirectX::ScratchImage compressed_image;
    HRESULT hr = DirectX::Compress(
        output_image.GetImages(),
        output_image.GetImageCount(),
        output_image.GetMetadata(),
        DXGI_FORMAT_BC3_UNORM,
        DirectX::TEX_COMPRESS_DEFAULT,
        1.0f,
        compressed_image);
    if (FAILED(hr)) {
        spdlog::debug("Failed to compress output DDS file: {}", getHRESULTErrorMessage(hr));
        return DirectX::ScratchImage();
    }

    return compressed_image;
}

//
// GPU Helpers
//
bool ParallaxGenD3D::isPowerOfTwo(unsigned int x) {
    return x && !(x & (x - 1));
}

ComPtr<ID3D11Texture2D> ParallaxGenD3D::createTexture2D(const DirectX::ScratchImage& texture) const
{
    // Define error object
    HRESULT hr;

    // Smart Pointer to hold texture for output
    ComPtr<ID3D11Texture2D> texture_out;

    // Verify dimention
    auto texture_meta = texture.GetMetadata();
    if (!isPowerOfTwo(static_cast<unsigned int>(texture_meta.width)) || !isPowerOfTwo(static_cast<unsigned int>(texture_meta.height))) {
        spdlog::debug("Texture dimensions must be a power of 2: {}x{}", texture_meta.width, texture_meta.height);
        return texture_out;
    }

    // Create texture
    hr = DirectX::CreateTexture(pDevice.Get(), texture.GetImages(), texture.GetImageCount(), texture.GetMetadata(), reinterpret_cast<ID3D11Resource**>(texture_out.
    ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return texture_out;
}

ComPtr<ID3D11Texture2D> ParallaxGenD3D::createTexture2D(ComPtr<ID3D11Texture2D>& existing_texture) const
{
    // Smart Pointer to hold texture for output
    D3D11_TEXTURE2D_DESC texture_out_desc;
    existing_texture->GetDesc(&texture_out_desc);

    // Create texture from description
    return createTexture2D(texture_out_desc);
}

ComPtr<ID3D11Texture2D> ParallaxGenD3D::createTexture2D(D3D11_TEXTURE2D_DESC& desc) const
{
    // Define error object
    HRESULT hr;

    // Smart Pointer to hold texture for output
    ComPtr<ID3D11Texture2D> texture_out;

    // Create texture
    hr = pDevice->CreateTexture2D(&desc, nullptr, texture_out.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return texture_out;
}

ComPtr<ID3D11ShaderResourceView> ParallaxGenD3D::createShaderResourceView(const ComPtr<ID3D11Texture2D>& texture) const
{
    // Define error object
    HRESULT hr;

    // Create SRV for texture
    D3D11_SHADER_RESOURCE_VIEW_DESC shader_desc = {};
    shader_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_desc.Texture2D.MipLevels = 1;

    // Smart Pointer to hold SRV for output
    ComPtr<ID3D11ShaderResourceView> srv_out;

    // Create SRV
    hr = pDevice->CreateShaderResourceView(texture.Get(), &shader_desc, srv_out.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11ShaderResourceView on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return srv_out;
}

ComPtr<ID3D11UnorderedAccessView> ParallaxGenD3D::createUnorderedAccessView(const ComPtr<ID3D11Texture2D>& texture) const
{
    // Define error object
    HRESULT hr;

    // Create UAV for texture
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    // Smart Pointer to hold UAV for output
    ComPtr<ID3D11UnorderedAccessView> uav_out;

    // Create UAV
    hr = pDevice->CreateUnorderedAccessView(texture.Get(), &uav_desc, uav_out.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11UnorderedAccessView on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return uav_out;
}

ComPtr<ID3D11UnorderedAccessView> ParallaxGenD3D::createUnorderedAccessView(ComPtr<ID3D11Resource> gpu_resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc) const
{
    HRESULT hr;

    ComPtr<ID3D11UnorderedAccessView> output_uav;
    hr = pDevice->CreateUnorderedAccessView(gpu_resource.Get(), &desc, &output_uav);
    if (FAILED(hr))
    {
        spdlog::debug("Failed to create ID3D11UnorderedAccessView on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return output_uav;
}

ComPtr<ID3D11Buffer> ParallaxGenD3D::createBuffer(const void* data, D3D11_BUFFER_DESC& desc) const
{
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ComPtr<ID3D11Buffer> output_buffer;
    HRESULT hr = pDevice->CreateBuffer(&desc, &initData, output_buffer.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        spdlog::debug("Failed to create ID3D11Buffer on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return output_buffer;
}

ComPtr<ID3D11Buffer> ParallaxGenD3D::createConstantBuffer(const void* data, const UINT size) const
{
    // Define error object
    HRESULT hr;

    // Create buffer
    D3D11_BUFFER_DESC cb_desc = {};
    cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    cb_desc.ByteWidth = (size + 16) - (size % 16);
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cb_desc.MiscFlags = 0;
    cb_desc.StructureByteStride = 0;

    // Create init data
    D3D11_SUBRESOURCE_DATA cb_InitData;
    cb_InitData.pSysMem = data;
    cb_InitData.SysMemPitch = 0;
    cb_InitData.SysMemSlicePitch = 0;

    // Smart Pointer to hold buffer for output
    ComPtr<ID3D11Buffer> buffer_out;

    // Create buffer
    hr = pDevice->CreateBuffer(&cb_desc, &cb_InitData, buffer_out.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Buffer on GPU: {}", getHRESULTErrorMessage(hr));
    }

    return buffer_out;
}

bool ParallaxGenD3D::BlockingDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) const
{
    // Error object
    HRESULT hr;

    // query
    ComPtr<ID3D11Query> pQuery = nullptr;
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_EVENT;
    hr = pDevice->CreateQuery(&queryDesc, pQuery.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create query: {}", getHRESULTErrorMessage(hr));
        return false;
    }

    // Run the shader
    pContext->Dispatch((ThreadGroupCountX + NUM_GPU_THREADS - 1) / NUM_GPU_THREADS, (ThreadGroupCountY + NUM_GPU_THREADS - 1), (ThreadGroupCountZ + NUM_GPU_THREADS - 1));

    // end query
    pContext->End(pQuery.Get());

    // wait for shader to complete
    BOOL queryData = FALSE;
    hr = pContext->GetData(pQuery.Get(), &queryData, sizeof(queryData), D3D11_ASYNC_GETDATA_DONOTFLUSH);  // block until complete
    if (FAILED(hr)) {
        spdlog::debug("Failed to get query data: {}", getHRESULTErrorMessage(hr));
        return false;
    }
    pQuery.Reset();

    // return success
    return true;
}

vector<unsigned char> ParallaxGenD3D::readBack(const ComPtr<ID3D11Texture2D>& gpu_resource, const int channels) const
{
    // Error object
    HRESULT hr;

    // grab edge detection results
    D3D11_TEXTURE2D_DESC staging_tex2d_desc;
    gpu_resource->GetDesc(&staging_tex2d_desc);
    // Enable flags for CPU access
    staging_tex2d_desc.Usage = D3D11_USAGE_STAGING;
    staging_tex2d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_tex2d_desc.BindFlags = 0;

    // create staging texture and copy resource
	ComPtr<ID3D11Texture2D> staging_tex2d = createTexture2D(staging_tex2d_desc);

    // copy resource
    pContext->CopyResource(staging_tex2d.Get(), gpu_resource.Get());

    // map resource to CPU
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = pContext->Map(staging_tex2d.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        spdlog::debug("[GPU] Failed to map resource to CPU during read back: {}", getHRESULTErrorMessage(hr));
        return vector<unsigned char>();
    }

    // Access the texture data from mappedResource.pData
    // TODO auto detect # of channels?
    size_t dataSize = staging_tex2d_desc.Width * staging_tex2d_desc.Height * channels;
    std::vector<unsigned char> output_data(reinterpret_cast<unsigned char*>(mappedResource.pData), reinterpret_cast<unsigned char*>(mappedResource.pData) + dataSize);

    // Cleaup
    pContext->Unmap(staging_tex2d.Get(), 0);  // cleanup map

    staging_tex2d.Reset();

    return output_data;
}

template<typename T>
vector<T> ParallaxGenD3D::readBack(const ComPtr<ID3D11Buffer>& gpu_resource) const
{
    // Error object
    HRESULT hr;

    // grab edge detection results
    D3D11_BUFFER_DESC buffer_desc;
    gpu_resource->GetDesc(&buffer_desc);
    // Enable flags for CPU access
    buffer_desc.Usage = D3D11_USAGE_STAGING;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    buffer_desc.BindFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;

    // Create the staging buffer
    ComPtr<ID3D11Buffer> staging_buffer;
    hr = pDevice->CreateBuffer(&buffer_desc, nullptr, staging_buffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("[GPU] Failed to create staging buffer: {}", getHRESULTErrorMessage(hr));
        return std::vector<T>();
    }

    // copy resource
    pContext->CopyResource(staging_buffer.Get(), gpu_resource.Get());

    // map resource to CPU
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = pContext->Map(staging_buffer.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        spdlog::debug("[GPU] Failed to map resource to CPU during read back: {}", getHRESULTErrorMessage(hr));
        return vector<T>();
    }

    // Access the texture data from mappedResource.pData
    size_t numElements = buffer_desc.ByteWidth / sizeof(T);
    std::vector<T> output_data(reinterpret_cast<T*>(mappedResource.pData), reinterpret_cast<T*>(mappedResource.pData) + numElements);

    // Cleaup
    pContext->Unmap(staging_buffer.Get(), 0);  // cleanup map

    staging_buffer.Reset();

    return output_data;
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
            spdlog::debug("Failed to load DDS file: {}", getHRESULTErrorMessage(hr));
            return false;
        }
    } else if(pgd->isBSAFile(dds_path)) {
        spdlog::trace(L"Reading DDS BSA file {}", dds_path.wstring());
        vector<std::byte> dds_bytes = pgd->getFile(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::LoadFromDDSMemory(dds_bytes.data(), dds_bytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, dds);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file from memory: {}", getHRESULTErrorMessage(hr));
            return false;
        }
    } else {
        spdlog::trace(L"Reading DDS file from output dir {}", dds_path.wstring());
        filesystem::path full_path = output_dir / dds_path;

        // Load DDS file
        HRESULT hr = DirectX::LoadFromDDSFile(full_path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, dds);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file: {}", getHRESULTErrorMessage(hr));
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
            spdlog::debug("Failed to load DDS file metadata: {}", getHRESULTErrorMessage(hr));
            return false;
        }
    } else if (pgd->isBSAFile(dds_path)) {
        spdlog::trace(L"Reading DDS BSA file metadata {}", dds_path.wstring());
        vector<std::byte> dds_bytes = pgd->getFile(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::GetMetadataFromDDSMemory(dds_bytes.data(), dds_bytes.size(), DirectX::DDS_FLAGS_NONE, dds_meta);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file metadata from memory: {}", getHRESULTErrorMessage(hr));
            return false;
        }
    } else {
        spdlog::trace(L"Reading DDS file from output dir {}", dds_path.wstring());
        filesystem::path full_path = output_dir / dds_path;

        // Load DDS file
        HRESULT hr = DirectX::GetMetadataFromDDSFile(full_path.c_str(), DirectX::DDS_FLAGS_NONE, dds_meta);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file metadata: {}", getHRESULTErrorMessage(hr));
            return false;
        }
    }

    return true;
}

DirectX::ScratchImage ParallaxGenD3D::LoadRawPixelsToScratchImage(const vector<unsigned char> rawPixels, size_t width, size_t height, DXGI_FORMAT format, int channels)
{
    // Initialize a ScratchImage
    DirectX::ScratchImage image;
    HRESULT hr = image.Initialize2D(format, width, height, 1, 1); // 1 array slice, 1 mipmap level
    if (FAILED(hr))
    {
        spdlog::debug("Failed to initialize ScratchImage: {}", getHRESULTErrorMessage(hr));
        return DirectX::ScratchImage();
    }

    // Get the image data
    const DirectX::Image* img = image.GetImage(0, 0, 0);
    if (!img) {
        spdlog::debug("Failed to get image data from ScratchImage");
        return DirectX::ScratchImage();
    }

    // Copy the raw pixel data into the image
    memcpy(img->pixels, rawPixels.data(), rawPixels.size());

    return image;
}

string ParallaxGenD3D::getHRESULTErrorMessage(HRESULT hr)
{
    // Get error message
    _com_error err(hr);
    return err.ErrorMessage();
}
