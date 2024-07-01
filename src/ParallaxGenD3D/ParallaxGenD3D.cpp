#include "ParallaxGenD3D/ParallaxGenD3D.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <cmath>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using Microsoft::WRL::ComPtr;

ParallaxGenD3D::ParallaxGenD3D(ParallaxGenDirectory* pgd)
{
    // Constructor
    this->pgd = pgd;

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

    CreateShaders();
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
    // GaussianBlurX.hlsl
    if (!createComputeShader(L"GaussianBlurX.cso", shader_GaussianBlurX)) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
    }

    // GaussianBlurY.hlsl
    if (!createComputeShader(L"GaussianBlurY.cso", shader_GaussianBlurY)) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
    }

    // Passthrough.hlsl
    if (!createComputeShader(L"Passthrough.cso", shader_Passthrough)) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
    }

    // Scaling.hlsl
    if (!createComputeShader(L"Scaling.cso", shader_Scaling)) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
    }

    // SobelEdgeDetection.hlsl
    if (!createComputeShader(L"SobelEdgeDetection.cso", shader_SobelEdgeDetection)) {
        spdlog::critical("Failed to create compute shader. Exiting.");
        ParallaxGenUtil::exitWithUserInput(1);
    }
}

bool ParallaxGenD3D::CheckHeightMapMatching(const std::filesystem::path& dds_path_1, const std::filesystem::path& dds_path_2) const
{
    // get metadata (should only pull headers, which is much faster)
    DirectX::TexMetadata dds_image_meta_1 = getDDSMetadata(dds_path_1);
    DirectX::TexMetadata dds_image_meta_2 = getDDSMetadata(dds_path_2);

    // calculate aspect ratios
    float aspect_ratio_1 = static_cast<float>(dds_image_meta_1.width) / static_cast<float>(dds_image_meta_1.height);
    float aspect_ratio_2 = static_cast<float>(dds_image_meta_2.width) / static_cast<float>(dds_image_meta_2.height);

    // check if aspect ratios don't match
    if (aspect_ratio_1 != aspect_ratio_2) {
        spdlog::trace("Aspect ratios don't match: {} vs {}", aspect_ratio_1, aspect_ratio_2);
        return false;
    }

    // Obtain edge detected images
    cv::Mat edge_1 = EdgeDetection(dds_path_1);
    cv::Mat edge_2 = EdgeDetection(dds_path_2);

    cv::imwrite("edge_1.png", edge_1);
    cv::imwrite("edge_2.png", edge_2);

    return true;
}

cv::Mat ParallaxGenD3D::EdgeDetection(const std::filesystem::path& dds_path) const
{
    // Get DDS file
    DirectX::ScratchImage dds = getDDS(dds_path);
    DirectX::TexMetadata dds_meta = dds.GetMetadata();

    // Create GPU texture object and SRV
    ComPtr<ID3D11Texture2D> dds_tex2D = createTexture2D(dds);
    ComPtr<ID3D11ShaderResourceView> dds_srv = createShaderResourceView(dds_tex2D);

    //
    // 1. Scale down the image
    //
    uint new_X = 512;
    uint new_Y = 512;

    D3D11_TEXTURE2D_DESC dds_scaled_tex2d_desc;
    dds_tex2D->GetDesc(&dds_scaled_tex2d_desc);
    dds_scaled_tex2d_desc.Width = new_X;
    dds_scaled_tex2d_desc.Height = new_Y;
    dds_scaled_tex2d_desc.MipLevels = 1;
    dds_scaled_tex2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dds_scaled_tex2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> dds_scaled_tex2D = createTexture2D(dds_scaled_tex2d_desc);
    ComPtr<ID3D11UnorderedAccessView> dds_scaled_uav = createUnorderedAccessView(dds_scaled_tex2D);

    dispatchShader_Scaling(dds_srv, dds_scaled_uav, static_cast<uint>(dds_meta.width), static_cast<uint>(dds_meta.height), new_X, new_Y);

    dds_tex2D.Reset();
    dds_srv.Reset();
    dds_scaled_uav.Reset();

    // Create SRV
    ComPtr<ID3D11ShaderResourceView> dds_scaled_srv = createShaderResourceView(dds_scaled_tex2D);

    //
    // 2. Gaussian Blur
    //

    // X Direction
    /*
    ComPtr<ID3D11Texture2D> dds_gaussianX_tex2D = createTexture2D(dds_scaled_tex2D);
    ComPtr<ID3D11UnorderedAccessView> dds_gaussianX_uav = createUnorderedAccessView(dds_gaussianX_tex2D);

    dispatchShader_GaussianBlurX(dds_scaled_srv, dds_gaussianX_uav, new_X, new_Y, 3, 1.0f);

    // Create SRV
    ComPtr<ID3D11ShaderResourceView> dds_gaussianX_srv = createShaderResourceView(dds_gaussianX_tex2D);

    // Y Direction
    ComPtr<ID3D11Texture2D> dds_gaussianY_tex2D = createTexture2D(dds_scaled_tex2D);
    ComPtr<ID3D11UnorderedAccessView> dds_gaussianY_uav = createUnorderedAccessView(dds_gaussianY_tex2D);

    dispatchShader_GaussianBlurY(dds_gaussianX_srv, dds_gaussianY_uav, new_X, new_Y, 3, 1.0f);

    // Create SRV
    ComPtr<ID3D11ShaderResourceView> dds_gaussianY_srv = createShaderResourceView(dds_gaussianY_tex2D);
    */

    //
    // 3. Sobel Edge Detection
    //

    D3D11_TEXTURE2D_DESC dds_sobeledgedetection_tex2d_desc;
    dds_scaled_tex2D->GetDesc(&dds_sobeledgedetection_tex2d_desc);
    dds_sobeledgedetection_tex2d_desc.Format = DXGI_FORMAT_R8_UNORM;
    ComPtr<ID3D11Texture2D> dds_edge_tex2D = createTexture2D(dds_sobeledgedetection_tex2d_desc);
    ComPtr<ID3D11UnorderedAccessView> dds_edge_uav = createUnorderedAccessView(dds_edge_tex2D);

    //dispatchShader_SobelEdgeDetection(dds_gaussianY_srv, dds_edge_uav, new_X, new_Y);
    dispatchShader_SobelEdgeDetection(dds_scaled_srv, dds_edge_uav, new_X, new_Y);

    dds_scaled_tex2D.Reset();
    dds_scaled_srv.Reset();
    dds_edge_uav.Reset();

    //
    // 4. Read back data to CPU
    //

    // Cleanup
    cv::Mat out = readBackTexture(dds_edge_tex2D, CV_8UC1);

    dds_edge_tex2D.Reset();

    pContext->Flush();

    return out;
}

//
// Shaders
//

bool ParallaxGenD3D::dispatchShader_GaussianBlurX(
    const ComPtr<ID3D11ShaderResourceView>& input,
    const ComPtr<ID3D11UnorderedAccessView>& output,
    const unsigned int width,
    const unsigned int height,
    const int radius,
    const float sigma
    ) const
{
    // Create constants struct for shader
    ParallaxGenD3D::GaussianBlurStruct cBuffer_struct = { radius, sigma, width, height };

    // Create buffer
    ComPtr<ID3D11Buffer> cBuffer = createConstantBuffer(&cBuffer_struct, sizeof(ParallaxGenD3D::GaussianBlurStruct));

    // Set shader
    pContext->CSSetShader(shader_GaussianBlurX.Get(), nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, cBuffer.GetAddressOf());
    pContext->CSSetShaderResources(0, 1, input.GetAddressOf());
    pContext->CSSetUnorderedAccessViews(0, 1, output.GetAddressOf(), nullptr);

    // Dispatch
    bool dispatch_success = BlockingDispatch(width, height, 1);
    if (!dispatch_success) {
        spdlog::warn("[GPU] Failed to dispatch GaussianBlurX shader");
        return false;
    }

    // Cleanup
    ID3D11Buffer* nullBuffer[1] = { nullptr };
    pContext->CSSetConstantBuffers(0, 1, nullBuffer);
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    pContext->CSSetShaderResources(0, 1, nullSRV);
    ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
    pContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    return true;
}

bool ParallaxGenD3D::dispatchShader_GaussianBlurY(
    const ComPtr<ID3D11ShaderResourceView>& input,
    const ComPtr<ID3D11UnorderedAccessView>& output,
    const unsigned int width,
    const unsigned int height,
    const int radius,
    const float sigma
    ) const
{
    // Create constants struct for shader
    ParallaxGenD3D::GaussianBlurStruct cBuffer_struct = { radius, sigma, width, height };

    // Create buffer
    ComPtr<ID3D11Buffer> cBuffer = createConstantBuffer(&cBuffer_struct, sizeof(ParallaxGenD3D::GaussianBlurStruct));

    // Set shader
    pContext->CSSetShader(shader_GaussianBlurY.Get(), nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, cBuffer.GetAddressOf());
    pContext->CSSetShaderResources(0, 1, input.GetAddressOf());
    pContext->CSSetUnorderedAccessViews(0, 1, output.GetAddressOf(), nullptr);

    // Dispatch
    bool dispatch_success = BlockingDispatch(width, height, 1);
    if (!dispatch_success) {
        spdlog::warn("[GPU] Failed to dispatch GaussianBlurY shader");
        return false;
    }

    // Cleanup
    ID3D11Buffer* nullBuffer[1] = { nullptr };
    pContext->CSSetConstantBuffers(0, 1, nullBuffer);
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    pContext->CSSetShaderResources(0, 1, nullSRV);
    ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
    pContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    return true;
}

bool ParallaxGenD3D::dispatchShader_Passthrough(
    const ComPtr<ID3D11ShaderResourceView>& input,
    const ComPtr<ID3D11UnorderedAccessView>& output,
    const unsigned int width,
    const unsigned int height
    ) const
{
    // Set shader
    pContext->CSSetShader(shader_Passthrough.Get(), nullptr, 0);
    pContext->CSSetShaderResources(0, 1, input.GetAddressOf());
    pContext->CSSetUnorderedAccessViews(0, 1, output.GetAddressOf(), nullptr);

    // Dispatch
    bool dispatch_success = BlockingDispatch(width, height, 1);
    if (!dispatch_success) {
        spdlog::warn("[GPU] Failed to dispatch Passthrough shader");
        return false;
    }

    // Cleanup
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    pContext->CSSetShaderResources(0, 1, nullSRV);
    ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
    pContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    return true;
}

bool ParallaxGenD3D::dispatchShader_Scaling(
    const ComPtr<ID3D11ShaderResourceView>& input,
    const ComPtr<ID3D11UnorderedAccessView>& output,
    const unsigned int width,
    const unsigned int height,
    const unsigned int new_width,
    const unsigned int new_height
    ) const
{
    // Create constants struct for shader
    float scale_X = static_cast<float>(width) / static_cast<float>(new_width);
    float scale_Y = static_cast<float>(height) / static_cast<float>(new_height);
    ParallaxGenD3D::ScalingStruct cBuffer_struct = { scale_X, scale_Y };

    // Create buffer
    ComPtr<ID3D11Buffer> cBuffer = createConstantBuffer(&cBuffer_struct, sizeof(ParallaxGenD3D::ScalingStruct));

    // Set shader
    pContext->CSSetShader(shader_Scaling.Get(), nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, cBuffer.GetAddressOf());
    pContext->CSSetShaderResources(0, 1, input.GetAddressOf());
    pContext->CSSetUnorderedAccessViews(0, 1, output.GetAddressOf(), nullptr);

    // Dispatch
    bool dispatch_success = BlockingDispatch(new_width, new_height, 1);
    if (!dispatch_success) {
        spdlog::warn("[GPU] Failed to dispatch Scaling shader");
        return false;
    }

    // Cleanup
    ID3D11Buffer* nullBuffer[1] = { nullptr };
    pContext->CSSetConstantBuffers(0, 1, nullBuffer);
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    pContext->CSSetShaderResources(0, 1, nullSRV);
    ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
    pContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    return true;
}

bool ParallaxGenD3D::dispatchShader_SobelEdgeDetection(
    const ComPtr<ID3D11ShaderResourceView>& input,
    const ComPtr<ID3D11UnorderedAccessView>& output,
    const unsigned int width,
    const unsigned int height
    ) const
{
    // Create constants struct for shader
    ParallaxGenD3D::SobelEdgeDetectionStruct cBuffer_struct = { width - 1, height - 1 };

    // Create buffer
    ComPtr<ID3D11Buffer> cBuffer = createConstantBuffer(&cBuffer_struct, sizeof(ParallaxGenD3D::SobelEdgeDetectionStruct));

    // Set shader
    pContext->CSSetShader(shader_SobelEdgeDetection.Get(), nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, cBuffer.GetAddressOf());
    pContext->CSSetShaderResources(0, 1, input.GetAddressOf());
    pContext->CSSetUnorderedAccessViews(0, 1, output.GetAddressOf(), nullptr);

    // Dispatch
    bool dispatch_success = BlockingDispatch(width, height, 1);
    if (!dispatch_success) {
        spdlog::warn("[GPU] Failed to dispatch SobelEdgeDetection shader");
        return false;
    }

    // Cleanup
    ID3D11Buffer* nullBuffer[1] = { nullptr };
    pContext->CSSetConstantBuffers(0, 1, nullBuffer);
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    pContext->CSSetShaderResources(0, 1, nullSRV);
    ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
    pContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    return true;
}

//
// GPU Helpers
//

ComPtr<ID3D11Texture2D> ParallaxGenD3D::createTexture2D(const DirectX::ScratchImage& texture) const
{
    // Define error object
    HRESULT hr;

    // Smart Pointer to hold texture for output
    ComPtr<ID3D11Texture2D> texture_out;

    // Create texture
    hr = DirectX::CreateTexture(pDevice.Get(), texture.GetImages(), texture.GetImageCount(), texture.GetMetadata(), reinterpret_cast<ID3D11Resource**>(texture_out.
    ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        // Log on failure
        spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", hr);
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
        spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", hr);
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
        spdlog::debug("Failed to create ID3D11ShaderResourceView on GPU: {}", hr);
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
        spdlog::debug("Failed to create ID3D11UnorderedAccessView on GPU: {}", hr);
    }

    return uav_out;
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
        spdlog::debug("Failed to create ID3D11Buffer on GPU: {}", hr);
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
        spdlog::debug("Failed to create query: {}", hr);
        return false;
    }

    // Run the shader
    pContext->Dispatch((ThreadGroupCountX + num_gpu_threads - 1) / num_gpu_threads, (ThreadGroupCountY + num_gpu_threads - 1), (ThreadGroupCountZ + num_gpu_threads - 1));

    // end query
    pContext->End(pQuery.Get());

    // wait for shader to complete
    BOOL queryData = FALSE;
    pContext->GetData(pQuery.Get(), &queryData, sizeof(queryData), D3D11_ASYNC_GETDATA_DONOTFLUSH);  // block until complete
    pQuery.Reset();

    // return success
    return true;
}

cv::Mat ParallaxGenD3D::readBackTexture(const ComPtr<ID3D11Texture2D>& gpu_resource, int type) const
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
        spdlog::debug("[GPU] Failed to map resource to CPU during read back: {}", hr);
        return cv::Mat();
    }

    // Access the texture data from mappedResource.pData
    cv::Mat output_mat = cv::Mat(staging_tex2d_desc.Width, staging_tex2d_desc.Height, type, reinterpret_cast<unsigned char*>(mappedResource.pData)).clone();

    // Cleaup
    pContext->Unmap(staging_tex2d.Get(), 0);  // cleanup map

    staging_tex2d.Reset();

    return output_mat;
}

bool ParallaxGenD3D::createComputeShader(const wstring& shader_path, ComPtr<ID3D11ComputeShader>& cs_dest) const
{
    // Load shader
    auto shader_bytes = LoadCompiledShader(shader_path);

    // Create shader
    HRESULT hr = pDevice->CreateComputeShader(shader_bytes.data(), shader_bytes.size(), nullptr, cs_dest.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::debug("Failed to create compute shader: {}", hr);
        return false;
    }

    return true;
}

//
// Texture Helpers
//

DirectX::ScratchImage ParallaxGenD3D::getDDS(const filesystem::path& dds_path) const
{
    DirectX::ScratchImage dds_image;

    if (pgd->isLooseFile(dds_path)) {
        spdlog::trace(L"Reading DDS loose file {}", dds_path.wstring());
        filesystem::path full_path = pgd->getFullPath(dds_path);

        // Load DDS file
        // ! Todo: wstring support for the path here?
        HRESULT hr = DirectX::LoadFromDDSFile(full_path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, dds_image);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file: {}", hr);
            return DirectX::ScratchImage();
        }
    } else {
        spdlog::trace(L"Reading DDS BSA file {}", dds_path.wstring());
        vector<std::byte> dds_bytes = pgd->getFile(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::LoadFromDDSMemory(dds_bytes.data(), dds_bytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, dds_image);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file from memory: {}", hr);
            return DirectX::ScratchImage();
        }
    }

    return dds_image;
}

DirectX::TexMetadata ParallaxGenD3D::getDDSMetadata(const filesystem::path& dds_path) const
{
    DirectX::TexMetadata dds_image_meta;

    if (pgd->isLooseFile(dds_path)) {
        spdlog::trace(L"Reading DDS loose file metadata {}", dds_path.wstring());
        filesystem::path full_path = pgd->getFullPath(dds_path);

        // Load DDS file
        // ! Todo: wstring support for the path here?
        HRESULT hr = DirectX::GetMetadataFromDDSFile(full_path.c_str(), DirectX::DDS_FLAGS_NONE, dds_image_meta);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file metadata: {}", hr);
            return DirectX::TexMetadata();
        }
    } else {
        spdlog::trace(L"Reading DDS BSA file metadata {}", dds_path.wstring());
        vector<std::byte> dds_bytes = pgd->getFile(dds_path);

        // Load DDS file
        HRESULT hr = DirectX::GetMetadataFromDDSMemory(dds_bytes.data(), dds_bytes.size(), DirectX::DDS_FLAGS_NONE, dds_image_meta);
        if (FAILED(hr)) {
            spdlog::debug("Failed to load DDS file metadata from memory: {}", hr);
            return DirectX::TexMetadata();
        }
    }

    return dds_image_meta;
}