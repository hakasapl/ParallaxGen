#include "ParallaxGenD3D.hpp"

#include <DirectXTex.h>
#include <algorithm>
#include <comdef.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <spdlog/spdlog.h>
#include <winnt.h>

#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using Microsoft::WRL::ComPtr;

ParallaxGenD3D::ParallaxGenD3D(ParallaxGenDirectory *PGD, filesystem::path OutputDir, filesystem::path ExePath,
                               const bool &UseGPU)
    : PGD(PGD), OutputDir(std::move(OutputDir)), ExePath(std::move(ExePath)), UseGPU(UseGPU) {}

auto ParallaxGenD3D::findCMMaps(const std::unordered_set<std::wstring>& BSAExcludes) -> ParallaxGenTask::PGResult {
  auto &EnvMasks = PGD->getTextureMap(NIFUtil::TextureSlots::ENVMASK);

  ParallaxGenTask::PGResult PGResult = ParallaxGenTask::PGResult::SUCCESS;

  // loop through maps
  for (auto &EnvMask : EnvMasks) {
    bool bFileInVanillaBSA = PGD->isFileInBSA(EnvMask.second.Path, BSAExcludes);

    bool Result = false;
    if (!bFileInVanillaBSA) {
      ParallaxGenTask::updatePGResult(PGResult, checkIfCM(EnvMask.second.Path, Result),
                                      ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
    } else {
      spdlog::trace(L"Envmask {} is contained in excluded BSA - skipping complex material check",
                    EnvMask.second.Path.wstring());
    }


    if (Result) {
      // TODO we need to fill in alpha for non-CM stuff
      EnvMask.second.Type = NIFUtil::TextureType::COMPLEXMATERIAL;
      spdlog::trace(L"Found complex material env mask: {}", EnvMask.second.Path.wstring());
    }
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::checkIfCM(const filesystem::path &DDSPath, bool &Result) -> ParallaxGenTask::PGResult {
  // get metadata (should only pull headers, which is much faster)
  DirectX::TexMetadata DDSImageMeta{};
  auto PGResult = getDDSMetadata(DDSPath, DDSImageMeta);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return PGResult;
  }

  // If Alpha is opaque move on
  if (DDSImageMeta.GetAlphaMode() == DirectX::TEX_ALPHA_MODE_OPAQUE) {
    Result = false;
    return ParallaxGenTask::PGResult::SUCCESS;
  }

  bool BCCompressed = false;
  // Only check DDS with alpha channels
  switch (DDSImageMeta.format) {
  case DXGI_FORMAT_BC2_UNORM:
  case DXGI_FORMAT_BC2_UNORM_SRGB:
  case DXGI_FORMAT_BC2_TYPELESS:
  case DXGI_FORMAT_BC3_UNORM:
  case DXGI_FORMAT_BC3_UNORM_SRGB:
  case DXGI_FORMAT_BC3_TYPELESS:
  case DXGI_FORMAT_BC7_UNORM:
  case DXGI_FORMAT_BC7_UNORM_SRGB:
  case DXGI_FORMAT_BC7_TYPELESS:
    BCCompressed = true;
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
    break;
  default:
    Result = false;
    return ParallaxGenTask::PGResult::SUCCESS;
  }

  // Read image
  DirectX::ScratchImage Image;
  PGResult = getDDS(DDSPath, Image);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    spdlog::error(L"Failed to load DDS file (Skipping): {}", DDSPath.wstring());
    return PGResult;
  }

  int AlphaValues = -1;
  if (UseGPU) {
    // GPU
    AlphaValues = countAlphaValuesGPU(Image);
  } else {
    // CPU
    AlphaValues = countAlphaValuesCPU(Image, BCCompressed);
  }

  size_t NumPixels = DDSImageMeta.width * DDSImageMeta.height;
  if (AlphaValues > NumPixels / 2) {
    Result = false;
    return ParallaxGenTask::PGResult::SUCCESS;
  }

  Result = true;
  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::countAlphaValuesGPU(const DirectX::ScratchImage &Image) -> int {
  // Create GPU texture
  ComPtr<ID3D11Texture2D> InputTex;
  if (createTexture2D(Image, InputTex) != ParallaxGenTask::PGResult::SUCCESS) {
    return -1;
  }

  // Create SRV
  ComPtr<ID3D11ShaderResourceView> InputSRV;
  if (createShaderResourceView(InputTex, InputSRV) != ParallaxGenTask::PGResult::SUCCESS) {
    return -1;
  }

  // Create buffer for output
  D3D11_BUFFER_DESC BufferDesc = {};
  BufferDesc.Usage = D3D11_USAGE_DEFAULT;
  BufferDesc.ByteWidth = sizeof(UINT);
  BufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  BufferDesc.CPUAccessFlags = 0;
  BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  BufferDesc.StructureByteStride = sizeof(UINT);

  unsigned int OutputData = 0;
  ComPtr<ID3D11Buffer> OutputBuffer;
  if (createBuffer(&OutputData, BufferDesc, OutputBuffer) != ParallaxGenTask::PGResult::SUCCESS) {
    return -1;
  }

  // Create UAV for output buffer
  D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
  UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
  UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  UAVDesc.Buffer.FirstElement = 0; // NOLINT
  UAVDesc.Buffer.NumElements = 1;  // NOLINT

  ComPtr<ID3D11UnorderedAccessView> OutputBufferUAV;
  if (createUnorderedAccessView(OutputBuffer, UAVDesc, OutputBufferUAV) != ParallaxGenTask::PGResult::SUCCESS) {
    return -1;
  }

  // Dispatch shader
  PtrContext->CSSetShader(ShaderCountAlphaValues.Get(), nullptr, 0);
  PtrContext->CSSetShaderResources(0, 1, InputSRV.GetAddressOf());
  PtrContext->CSSetUnorderedAccessViews(0, 1, OutputBufferUAV.GetAddressOf(), nullptr);

  DirectX::TexMetadata ImageMeta = Image.GetMetadata();
  if (BlockingDispatch(static_cast<UINT>(ImageMeta.width), static_cast<UINT>(ImageMeta.height), 1) !=
      ParallaxGenTask::PGResult::SUCCESS) {
    return -1;
  }

  // Clean Up Objects
  InputTex.Reset();
  InputSRV.Reset();

  // Clean up shader resources
  ID3D11ShaderResourceView *NullSRV[] = {nullptr, nullptr};
  PtrContext->CSSetShaderResources(0, 1, NullSRV);
  ID3D11UnorderedAccessView *NullUAV[] = {nullptr, nullptr};
  PtrContext->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);
  ID3D11ComputeShader *NullShader[] = {nullptr};
  PtrContext->CSSetShader(NullShader[0], nullptr, 0);

  // Read back data
  vector<unsigned int> Data = readBack<unsigned int>(OutputBuffer);

  // Cleanup
  OutputBuffer.Reset();
  OutputBufferUAV.Reset();

  PtrContext->Flush(); // Flush GPU to avoid leaks

  return static_cast<int>(Data[0]);
}

auto ParallaxGenD3D::countAlphaValuesCPU(const DirectX::ScratchImage &Image, const bool &BCCompressed) -> int {
  // Decompress image if needed
  HRESULT HR{};
  DirectX::ScratchImage InImage;
  if (BCCompressed) {
    HR = DirectX::Decompress(Image.GetImages(), Image.GetImageCount(), Image.GetMetadata(), DXGI_FORMAT_R8G8B8A8_UNORM,
                             InImage);
    if (FAILED(HR)) {
      spdlog::error("Failed to decompress DDS file (Skipping): {}", getHRESULTErrorMessage(HR));
      return -1;
    }
  } else {
    InImage.Initialize2D(Image.GetMetadata().format, Image.GetMetadata().width, Image.GetMetadata().height, 1, 1);
    memcpy(InImage.GetPixels(), Image.GetPixels(), Image.GetPixelsSize());
  }

  // Find Row Pitch
  size_t RowPitch = 0;
  size_t SlicePitch = 0;
  HR = DirectX::ComputePitch(DXGI_FORMAT_R8G8B8A8_UNORM, InImage.GetMetadata().width, InImage.GetMetadata().height,
                             RowPitch, SlicePitch);
  if (FAILED(HR)) {
    spdlog::error("Failed to compute pitch for DDS file (Skipping): {}", getHRESULTErrorMessage(HR));
    return -1;
  }

  // Calculate Median of Alpha Layer
  int AlphaValues = 0;

  const auto *Pixels = InImage.GetPixels();
  for (size_t Y = 0; Y < InImage.GetMetadata().height; ++Y) {
    for (size_t X = 0; X < InImage.GetMetadata().width; ++X) {
      size_t PixelIndex = (Y * RowPitch) + (X * 4); // Assuming 4 bytes per pixel (RGBA)
      uint8_t Alpha = Pixels[PixelIndex + 3];       // NOLINT
      if (Alpha == 255) {                           // NOLINT
        AlphaValues++;
      }
    }
  }

  return AlphaValues;
}

auto ParallaxGenD3D::checkIfAspectRatioMatches(const std::filesystem::path &DDSPath1,
                                               const std::filesystem::path &DDSPath2,
                                               bool &CheckAspect) -> ParallaxGenTask::PGResult {

  ParallaxGenTask::PGResult PGResult{};

  // get metadata (should only pull headers, which is much faster)
  DirectX::TexMetadata DDSImageMeta1{};
  PGResult = getDDSMetadata(DDSPath1, DDSImageMeta1);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return PGResult;
  }

  DirectX::TexMetadata DDSImageMeta2{};
  PGResult = getDDSMetadata(DDSPath2, DDSImageMeta2);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return PGResult;
  }

  // calculate aspect ratios
  float AspectRatio1 = static_cast<float>(DDSImageMeta1.width) / static_cast<float>(DDSImageMeta1.height);
  float AspectRatio2 = static_cast<float>(DDSImageMeta2.width) / static_cast<float>(DDSImageMeta2.height);

  // check if aspect ratios don't match
  CheckAspect = AspectRatio1 == AspectRatio2;

  return ParallaxGenTask::PGResult::SUCCESS;
}

//
// GPU Code
//

void ParallaxGenD3D::initGPU() {
// initialize GPU device and context
#ifdef _DEBUG
  UINT DeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
  UINT DeviceFlags = 0;
#endif

  HRESULT HR{};
  HR = D3D11CreateDevice(nullptr,                  // Adapter (configured to use default)
                         D3D_DRIVER_TYPE_HARDWARE, // User Hardware
                         0,                        // Irrelevant for hardware driver type
                         DeviceFlags,              // Device flags (enables debug if needed)
                         &FeatureLevel,            // Feature levels this app can support
                         1,                        // Just 1 feature level (D3D11)
                         D3D11_SDK_VERSION,        // Set to D3D11 SDK version
                         &PtrDevice,               // Sets the instance device
                         nullptr,                  // Returns feature level of device created (not needed)
                         &PtrContext               // Sets the instance device immediate context
  );

  // check if device was found successfully
  if (FAILED(HR)) {
    spdlog::error("D3D11 device creation failure error: {}", getHRESULTErrorMessage(HR));
    spdlog::critical("Unable to find any DX11 capable devices. Disable any GPU-accelerated "
                     "features to continue.");
    exit(1);
  }

  // Init Shaders
  initShaders();
}

void ParallaxGenD3D::initShaders() {
  ParallaxGenTask::PGResult PGResult{};

  // TODO don't repeat error handling code
  // MergeToComplexMaterial.hlsl
  PGResult = createComputeShader(L"MergeToComplexMaterial.hlsl", ShaderMergeToComplexMaterial);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    spdlog::critical("Failed to create compute shader. Exiting.");
    exit(1);
  }

  // CountAlphaValues.hlsl
  PGResult = createComputeShader(L"CountAlphaValues.hlsl", ShaderCountAlphaValues);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    spdlog::critical("Failed to create compute shader. Exiting.");
    exit(1);
  }
}

auto ParallaxGenD3D::compileShader(const std::filesystem::path &Filename,
                                   ComPtr<ID3DBlob> &ShaderBlob) const -> ParallaxGenTask::PGResult {
  spdlog::trace(L"Starting compiling shader: {}", Filename.wstring());

  DWORD DWShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
  DWShaderFlags |= D3DCOMPILE_DEBUG;
#endif

  filesystem::path ShaderAbsPath = ExePath / "shaders" / Filename;

  ComPtr<ID3DBlob> PtrErrorBlob;
  HRESULT HR = D3DCompileFromFile(ShaderAbsPath.c_str(), nullptr, nullptr, "main", "cs_5_0", DWShaderFlags, 0,
                                  ShaderBlob.ReleaseAndGetAddressOf(), PtrErrorBlob.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    if (PtrErrorBlob != nullptr) {
      spdlog::critical(L"Failed to compile shader {}: {}, {}", Filename.wstring(),
                       strToWstr(getHRESULTErrorMessage(HR)),
                       static_cast<wchar_t *>(PtrErrorBlob->GetBufferPointer()));
      PtrErrorBlob.Reset();
    } else {
      spdlog::critical(L"Failed to compile shader {}: {}", Filename.wstring(),
                       strToWstr(getHRESULTErrorMessage(HR)));
    }

    exit(1);
  }

  spdlog::debug(L"Shader {} compiled successfully", Filename.wstring());

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createComputeShader(const wstring &ShaderPath,
                                         ComPtr<ID3D11ComputeShader> &ShaderDest) -> ParallaxGenTask::PGResult {
  // Load shader
  ComPtr<ID3DBlob> CompiledShader;
  compileShader(ShaderPath, CompiledShader);

  // Create shader
  HRESULT HR = PtrDevice->CreateComputeShader(CompiledShader->GetBufferPointer(), CompiledShader->GetBufferSize(),
                                              nullptr, ShaderDest.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    spdlog::debug("Failed to create compute shader: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::upgradeToComplexMaterial(const std::filesystem::path &ParallaxMap,
                                              const std::filesystem::path &EnvMap) const -> DirectX::ScratchImage {

  ParallaxGenTask::PGResult PGResult{};

  bool ParallaxExists = !ParallaxMap.empty();
  bool EnvExists = !EnvMap.empty();

  // Check if any texture was supplied
  if (!ParallaxExists && !EnvExists) {
    spdlog::error(L"Unable to upgrade to complex material, no textures supplied. {} and "
                  L"{}",
                  ParallaxMap.wstring(), EnvMap.wstring());
    return {};
  }

  // get parallax map
  DirectX::ScratchImage ParallaxMapDDS;
  if (ParallaxExists && getDDS(ParallaxMap, ParallaxMapDDS) != ParallaxGenTask::PGResult::SUCCESS) {
    spdlog::error(L"Failed to load DDS file: {}", ParallaxMap.wstring());
    return {};
  }
  DirectX::TexMetadata ParallaxMeta = ParallaxMapDDS.GetMetadata();

  // get env map
  DirectX::ScratchImage EnvMapDDS;
  if (EnvExists && getDDS(EnvMap, EnvMapDDS) != ParallaxGenTask::PGResult::SUCCESS) {
    spdlog::error(L"Failed to load DDS file: {}", ParallaxMap.wstring());
    return {};
  }
  DirectX::TexMetadata EnvMeta = EnvMapDDS.GetMetadata();

  // Check dimensions
  size_t ParallaxHeight = 0;
  size_t ParallaxWidth = 0;
  float ParallaxAspectRatio = 0.0F;
  if (ParallaxExists) {
    ParallaxHeight = ParallaxMeta.height;
    ParallaxWidth = ParallaxMeta.width;
    ParallaxAspectRatio = static_cast<float>(ParallaxWidth) / static_cast<float>(ParallaxHeight);
  }

  size_t EnvHeight = 0;
  size_t EnvWidth = 0;
  float EnvAspectRatio = 0.0F;
  if (EnvExists) {
    EnvHeight = EnvMeta.height;
    EnvWidth = EnvMeta.width;
    EnvAspectRatio = static_cast<float>(EnvWidth) / static_cast<float>(EnvHeight);
  }

  // Compare aspect ratios if both exist
  if (ParallaxExists && EnvExists && ParallaxAspectRatio != EnvAspectRatio) {
    spdlog::trace(L"Rejecting shader upgrade for {} due to aspect ratio mismatch with {}", ParallaxMap.wstring(),
                  EnvMap.wstring());
    return {};
  }

  // Create GPU Textures objects
  ComPtr<ID3D11Texture2D> ParallaxMapGPU;
  ComPtr<ID3D11ShaderResourceView> ParallaxMapSRV;
  if (ParallaxExists) {
    PGResult = createTexture2D(ParallaxMapDDS, ParallaxMapGPU);
    if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
      spdlog::error(L"Failed to create GPU texture for {}", ParallaxMap.wstring());
      return {};
    }
    PGResult = createShaderResourceView(ParallaxMapGPU, ParallaxMapSRV);
    if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
      spdlog::error(L"Failed to create GPU SRV for {}", ParallaxMap.wstring());
      return {};
    }
  }
  ComPtr<ID3D11Texture2D> EnvMapGPU;
  ComPtr<ID3D11ShaderResourceView> EnvMapSRV;
  if (EnvExists) {
    PGResult = createTexture2D(EnvMapDDS, EnvMapGPU);
    if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
      return {};
    }
    PGResult = createShaderResourceView(EnvMapGPU, EnvMapSRV);
    if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
      return {};
    }
  }

  // Define parameters in constant buffer
  UINT ResultWidth = static_cast<UINT>(max(ParallaxWidth, EnvWidth));
  UINT ResultHeight = static_cast<UINT>(max(ParallaxHeight, EnvHeight));

  // calculate scaling weights
  float ParallaxScalingWidth = 1.0F;
  float ParallaxScalingHeight = 1.0F;
  float EnvScalingWidth = 1.0F;
  float EnvScalingHeight = 1.0F;

  if (EnvExists && ParallaxExists) {
    ParallaxScalingWidth = static_cast<float>(ParallaxWidth) / static_cast<float>(ResultWidth);
    ParallaxScalingHeight = static_cast<float>(ParallaxHeight) / static_cast<float>(ResultHeight);
    EnvScalingWidth = static_cast<float>(EnvWidth) / static_cast<float>(ResultWidth);
    EnvScalingHeight = static_cast<float>(EnvHeight) / static_cast<float>(ResultHeight);
  }

  ShaderMergeToComplexMaterialParams ShaderParams{};
  ShaderParams.EnvMapScalingFactor = DirectX::XMFLOAT2(EnvScalingWidth, EnvScalingHeight);
  ShaderParams.ParallaxMapScalingFactor = DirectX::XMFLOAT2(ParallaxScalingWidth, ParallaxScalingHeight);
  ShaderParams.EnvMapAvailable = static_cast<BOOL>(EnvExists);
  ShaderParams.ParallaxMapAvailable = static_cast<BOOL>(ParallaxExists);
  ShaderParams.IntScalingFactor = MAX_CHANNEL_VALUE;

  ComPtr<ID3D11Buffer> ConstantBuffer;
  PGResult = createConstantBuffer(&ShaderParams, sizeof(ShaderMergeToComplexMaterialParams), ConstantBuffer);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }

  // Create output texture
  D3D11_TEXTURE2D_DESC OutputTextureDesc = {};
  if (ParallaxExists) {
    ParallaxMapGPU->GetDesc(&OutputTextureDesc);
  } else {
    EnvMapGPU->GetDesc(&OutputTextureDesc);
  }
  OutputTextureDesc.Width = ResultWidth;
  OutputTextureDesc.Height = ResultHeight;
  OutputTextureDesc.MipLevels = 0;
  OutputTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  OutputTextureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

  ComPtr<ID3D11Texture2D> OutputTexture;
  PGResult = createTexture2D(OutputTextureDesc, OutputTexture);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }
  // Query the texture's description to get the actual number of mip levels
  D3D11_TEXTURE2D_DESC CreatedOutputTextureDesc = {};
  OutputTexture->GetDesc(&CreatedOutputTextureDesc);
  UINT ResultMips = CreatedOutputTextureDesc.MipLevels;

  ComPtr<ID3D11UnorderedAccessView> OutputUAV;
  PGResult = createUnorderedAccessView(OutputTexture, OutputUAV);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }

  // Create buffer for output
  ShaderMergeToComplexMaterialOutputBuffer MinMaxInitData = {UINT_MAX, 0, UINT_MAX, 0};

  D3D11_BUFFER_DESC BufferDesc = {};
  BufferDesc.Usage = D3D11_USAGE_DEFAULT;
  BufferDesc.ByteWidth = sizeof(ShaderMergeToComplexMaterialOutputBuffer);
  BufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
  BufferDesc.CPUAccessFlags = 0;
  BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  BufferDesc.StructureByteStride = sizeof(UINT);

  ComPtr<ID3D11Buffer> OutputBuffer;
  PGResult = createBuffer(&MinMaxInitData, BufferDesc, OutputBuffer);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }

  // Create UAV for output buffer
  D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
  UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
  UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  UAVDesc.Buffer.FirstElement = 0; // NOLINT
  UAVDesc.Buffer.NumElements = 4;  // NOLINT

  ComPtr<ID3D11UnorderedAccessView> OutputBufferUAV;
  PGResult = createUnorderedAccessView(OutputBuffer, UAVDesc, OutputBufferUAV);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }

  // Dispatch shader
  PtrContext->CSSetShader(ShaderMergeToComplexMaterial.Get(), nullptr, 0);
  PtrContext->CSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
  PtrContext->CSSetShaderResources(0, 1, EnvMapSRV.GetAddressOf());
  PtrContext->CSSetShaderResources(1, 1, ParallaxMapSRV.GetAddressOf());
  PtrContext->CSSetUnorderedAccessViews(0, 1, OutputUAV.GetAddressOf(), nullptr);
  PtrContext->CSSetUnorderedAccessViews(1, 1, OutputBufferUAV.GetAddressOf(), nullptr);

  PGResult = BlockingDispatch(ResultWidth, ResultHeight, 1);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }

  // Clean up shader resources
  ID3D11ShaderResourceView *NullSRV[] = {nullptr, nullptr};
  PtrContext->CSSetShaderResources(0, 2, NullSRV);
  ID3D11UnorderedAccessView *NullUAV[] = {nullptr, nullptr};
  PtrContext->CSSetUnorderedAccessViews(0, 2, NullUAV, nullptr);
  ID3D11Buffer *NullBuffer[] = {nullptr};
  PtrContext->CSSetConstantBuffers(0, 1, NullBuffer);
  ID3D11ComputeShader *NullShader[] = {nullptr};
  PtrContext->CSSetShader(NullShader[0], nullptr, 0);

  // Release some objects
  ParallaxMapGPU.Reset();
  EnvMapGPU.Reset();
  ParallaxMapSRV.Reset();
  EnvMapSRV.Reset();
  ConstantBuffer.Reset();

  // Generate mips
  D3D11_TEXTURE2D_DESC OutputTextureMipsDesc = {};
  OutputTexture->GetDesc(&OutputTextureMipsDesc);
  OutputTextureMipsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  OutputTextureMipsDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
  ComPtr<ID3D11Texture2D> OutputTextureMips;
  PGResult = createTexture2D(OutputTextureMipsDesc, OutputTextureMips);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }
  ComPtr<ID3D11ShaderResourceView> OutputMipsSRV;
  PGResult = createShaderResourceView(OutputTextureMips, OutputMipsSRV);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
    return {};
  }

  // Copy texture
  PtrContext->CopyResource(OutputTextureMips.Get(), OutputTexture.Get());
  PtrContext->GenerateMips(OutputMipsSRV.Get());
  PtrContext->CopyResource(OutputTexture.Get(), OutputTextureMips.Get());

  // cleanup
  OutputTextureMips.Reset();
  OutputMipsSRV.Reset();

  // Read back output buffer
  auto OutputBufferData = readBack<UINT>(OutputBuffer);

  // Read back texture (DEBUG)
  auto OutputTextureData = readBack(OutputTexture, 4);

  // More cleaning
  OutputTexture.Reset();
  OutputUAV.Reset();
  OutputBuffer.Reset();
  OutputBufferUAV.Reset();

  // Flush GPU to avoid leaks
  PtrContext->Flush();

  // Import into directx scratchimage
  DirectX::ScratchImage OutputImage =
      loadRawPixelsToScratchImage(OutputTextureData, ResultWidth, ResultHeight, ResultMips, DXGI_FORMAT_R8G8B8A8_UNORM);

  // Compress DDS
  // BC3 works best with heightmaps
  DirectX::ScratchImage CompressedImage;
  HRESULT HR = DirectX::Compress(OutputImage.GetImages(), OutputImage.GetImageCount(), OutputImage.GetMetadata(),
                                 DXGI_FORMAT_BC3_UNORM, DirectX::TEX_COMPRESS_DEFAULT, 1.0F, CompressedImage);
  if (FAILED(HR)) {
    spdlog::error("Failed to compress output DDS file: {}", getHRESULTErrorMessage(HR));
    return {};
  }

  return CompressedImage;
}

//
// GPU Helpers
//
auto ParallaxGenD3D::isPowerOfTwo(unsigned int X) -> bool { return (X != 0U) && ((X & (X - 1)) == 0U); }

auto ParallaxGenD3D::createTexture2D(const DirectX::ScratchImage &Texture,
                                     ComPtr<ID3D11Texture2D> &Dest) const -> ParallaxGenTask::PGResult {
  // Define error object
  HRESULT HR{};

  // Verify dimention
  auto TextureMeta = Texture.GetMetadata();
  if (!isPowerOfTwo(static_cast<unsigned int>(TextureMeta.width)) ||
      !isPowerOfTwo(static_cast<unsigned int>(TextureMeta.height))) {
    spdlog::debug("Texture dimensions must be a power of 2: {}x{}", TextureMeta.width, TextureMeta.height);
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // Create texture
  HR = DirectX::CreateTexture(PtrDevice.Get(), Texture.GetImages(), Texture.GetImageCount(), Texture.GetMetadata(),
                              reinterpret_cast<ID3D11Resource **>(Dest.ReleaseAndGetAddressOf()));
  if (FAILED(HR)) {
    // Log on failure
    spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(HR));
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createTexture2D(ComPtr<ID3D11Texture2D> &ExistingTexture,
                                     ComPtr<ID3D11Texture2D> &Dest) const -> ParallaxGenTask::PGResult {
  // Smart Pointer to hold texture for output
  D3D11_TEXTURE2D_DESC TextureOutDesc;
  ExistingTexture->GetDesc(&TextureOutDesc);

  HRESULT HR{};

  // Create texture
  HR = PtrDevice->CreateTexture2D(&TextureOutDesc, nullptr, Dest.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    // Log on failure
    spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // Create texture from description
  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createTexture2D(D3D11_TEXTURE2D_DESC &Desc,
                                     ComPtr<ID3D11Texture2D> &Dest) const -> ParallaxGenTask::PGResult {
  // Define error object
  HRESULT HR{};

  // Create texture
  HR = PtrDevice->CreateTexture2D(&Desc, nullptr, Dest.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    // Log on failure
    spdlog::debug("Failed to create ID3D11Texture2D on GPU: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createShaderResourceView(
    const ComPtr<ID3D11Texture2D> &Texture, ComPtr<ID3D11ShaderResourceView> &Dest) const -> ParallaxGenTask::PGResult {
  // Define error object
  HRESULT HR{};

  // Create SRV for texture
  D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc = {};
  ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  ShaderDesc.Texture2D.MipLevels = -1;  // NOLINT

  // Create SRV
  HR = PtrDevice->CreateShaderResourceView(Texture.Get(), &ShaderDesc, Dest.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    // Log on failure
    spdlog::debug("Failed to create ID3D11ShaderResourceView on GPU: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createUnorderedAccessView(const ComPtr<ID3D11Texture2D> &Texture,
                                               ComPtr<ID3D11UnorderedAccessView> &Dest) const
    -> ParallaxGenTask::PGResult {
  // Define error object
  HRESULT HR{};

  // Create UAV for texture
  D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
  UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
  UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

  // Create UAV
  HR = PtrDevice->CreateUnorderedAccessView(Texture.Get(), &UAVDesc, Dest.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    // Log on failure
    spdlog::debug("Failed to create ID3D11UnorderedAccessView on GPU: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createUnorderedAccessView(
    const ComPtr<ID3D11Resource> &GPUResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC &Desc,
    ComPtr<ID3D11UnorderedAccessView> &Dest) const -> ParallaxGenTask::PGResult {
  HRESULT HR{};

  HR = PtrDevice->CreateUnorderedAccessView(GPUResource.Get(), &Desc, &Dest);
  if (FAILED(HR)) {
    spdlog::debug("Failed to create ID3D11UnorderedAccessView on GPU: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createBuffer(const void *Data, D3D11_BUFFER_DESC &Desc,
                                  ComPtr<ID3D11Buffer> &Dest) const -> ParallaxGenTask::PGResult {
  D3D11_SUBRESOURCE_DATA InitData = {};
  InitData.pSysMem = Data;

  HRESULT HR = PtrDevice->CreateBuffer(&Desc, &InitData, Dest.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    spdlog::debug("Failed to create ID3D11Buffer on GPU: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::createConstantBuffer(const void *Data, const UINT &Size,
                                          ComPtr<ID3D11Buffer> &Dest) const -> ParallaxGenTask::PGResult {
  // Define error object
  HRESULT HR{};

  // Create buffer
  D3D11_BUFFER_DESC CBDesc = {};
  CBDesc.Usage = D3D11_USAGE_DYNAMIC;
  CBDesc.ByteWidth = (Size + GPU_BUFFER_SIZE_MULTIPLE) - (Size % GPU_BUFFER_SIZE_MULTIPLE);
  CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  CBDesc.MiscFlags = 0;
  CBDesc.StructureByteStride = 0;

  // Create init data
  D3D11_SUBRESOURCE_DATA CBInitData;
  CBInitData.pSysMem = Data;
  CBInitData.SysMemPitch = 0;
  CBInitData.SysMemSlicePitch = 0;

  // Create buffer
  HR = PtrDevice->CreateBuffer(&CBDesc, &CBInitData, Dest.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    // Log on failure
    spdlog::debug("Failed to create ID3D11Buffer on GPU: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::BlockingDispatch(UINT THReadGroupCountX, UINT THReadGroupCountY,
                                      UINT THReadGroupCountZ) const -> ParallaxGenTask::PGResult {
  // Error object
  HRESULT HR{};

  // query
  ComPtr<ID3D11Query> PtrQuery = nullptr;
  D3D11_QUERY_DESC QueryDesc = {};
  QueryDesc.Query = D3D11_QUERY_EVENT;
  HR = PtrDevice->CreateQuery(&QueryDesc, PtrQuery.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    spdlog::debug("Failed to create query: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // Run the shader
  PtrContext->Dispatch((THReadGroupCountX + NUM_GPU_THREADS - 1) / NUM_GPU_THREADS,
                       (THReadGroupCountY + NUM_GPU_THREADS - 1) / NUM_GPU_THREADS,
                       (THReadGroupCountZ + NUM_GPU_THREADS - 1) / NUM_GPU_THREADS);

  // end query
  PtrContext->End(PtrQuery.Get());

  // wait for shader to complete
  BOOL QueryData = FALSE;
  HR = PtrContext->GetData(PtrQuery.Get(), &QueryData, sizeof(QueryData),
                           D3D11_ASYNC_GETDATA_DONOTFLUSH); // block until complete
  if (FAILED(HR)) {
    spdlog::debug("Failed to get query data: {}", getHRESULTErrorMessage(HR));
    return ParallaxGenTask::PGResult::FAILURE;
  }
  PtrQuery.Reset();

  // return success
  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::readBack(const ComPtr<ID3D11Texture2D> &GPUResource,
                              const int &Channels) const -> vector<unsigned char> {
  // Error object
  HRESULT HR{};
  ParallaxGenTask::PGResult PGResult{};

  // Grab texture description
  D3D11_TEXTURE2D_DESC StagingTex2DDesc;
  GPUResource->GetDesc(&StagingTex2DDesc);
  UINT MipLevels = StagingTex2DDesc.MipLevels;  // Number of mip levels to read back

  // Enable flags for CPU access
  StagingTex2DDesc.Usage = D3D11_USAGE_STAGING;
  StagingTex2DDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  StagingTex2DDesc.BindFlags = 0;
  StagingTex2DDesc.MiscFlags = 0;

  // Create staging texture
  ComPtr<ID3D11Texture2D> StagingTex2D;
  PGResult = createTexture2D(StagingTex2DDesc, StagingTex2D);
  if (PGResult != ParallaxGenTask::PGResult::SUCCESS) {
      spdlog::debug("Failed to create staging texture: {}", getHRESULTErrorMessage(HR));
      return {};
  }

  // Copy resource to staging texture
  PtrContext->CopyResource(StagingTex2D.Get(), GPUResource.Get());

  std::vector<unsigned char> OutputData;

  // Iterate over each mip level
  for (UINT MipLevel = 0; MipLevel < MipLevels; ++MipLevel) {
    // Get dimensions for the current mip level
    UINT MipWidth = std::max(1U, StagingTex2DDesc.Width >> MipLevel);
    UINT MipHeight = std::max(1U, StagingTex2DDesc.Height >> MipLevel);

    // Map the mip level to the CPU
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    HR = PtrContext->Map(StagingTex2D.Get(), MipLevel, D3D11_MAP_READ, 0, &MappedResource);
    if (FAILED(HR)) {
        spdlog::debug("[GPU] Failed to map resource to CPU during read back at mip level {}: {}", MipLevel, getHRESULTErrorMessage(HR));
        return {};
    }

    // Copy the data from the mapped resource to the output vector
    auto *SrcData = reinterpret_cast<unsigned char*>(MappedResource.pData);
    for (UINT Row = 0; Row < MipHeight; ++Row) {
        unsigned char* RowStart = SrcData + Row * MappedResource.RowPitch;  // NOLINT
        OutputData.insert(OutputData.end(), RowStart, RowStart + MipWidth * Channels);  // NOLINT
    }

    // Unmap the resource for this mip level
    PtrContext->Unmap(StagingTex2D.Get(), MipLevel);
  }

  StagingTex2D.Reset();

  return OutputData;
}

template <typename T> auto ParallaxGenD3D::readBack(const ComPtr<ID3D11Buffer> &GPUResource) const -> vector<T> {
  // Error object
  HRESULT HR{};

  // grab edge detection results
  D3D11_BUFFER_DESC BufferDesc;
  GPUResource->GetDesc(&BufferDesc);
  // Enable flags for CPU access
  BufferDesc.Usage = D3D11_USAGE_STAGING;
  BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  BufferDesc.BindFlags = 0;
  BufferDesc.MiscFlags = 0;
  BufferDesc.StructureByteStride = 0;

  // Create the staging buffer
  ComPtr<ID3D11Buffer> StagingBuffer;
  HR = PtrDevice->CreateBuffer(&BufferDesc, nullptr, StagingBuffer.ReleaseAndGetAddressOf());
  if (FAILED(HR)) {
    spdlog::debug("[GPU] Failed to create staging buffer: {}", getHRESULTErrorMessage(HR));
    return std::vector<T>();
  }

  // copy resource
  PtrContext->CopyResource(StagingBuffer.Get(), GPUResource.Get());

  // map resource to CPU
  D3D11_MAPPED_SUBRESOURCE MappedResource;
  HR = PtrContext->Map(StagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &MappedResource);
  if (FAILED(HR)) {
    spdlog::debug("[GPU] Failed to map resource to CPU during read back: {}", getHRESULTErrorMessage(HR));
    return vector<T>();
  }

  // Access the texture data from MappedResource.pData
  size_t NumElements = BufferDesc.ByteWidth / sizeof(T);
  std::vector<T> OutputData(reinterpret_cast<T *>(MappedResource.pData),
                            reinterpret_cast<T *>(MappedResource.pData) + NumElements);

  // Cleaup
  PtrContext->Unmap(StagingBuffer.Get(), 0); // cleanup map

  StagingBuffer.Reset();

  return OutputData;
}

//
// Texture Helpers
//

auto ParallaxGenD3D::getDDS(const filesystem::path &DDSPath,
                            DirectX::ScratchImage &DDS) const -> ParallaxGenTask::PGResult {
  HRESULT HR{};

  if (PGD->isLooseFile(DDSPath)) {
    spdlog::trace(L"Reading DDS loose file {}", DDSPath.wstring());
    filesystem::path FullPath = PGD->getFullPath(DDSPath);

    // Load DDS file
    HR = DirectX::LoadFromDDSFile(FullPath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, DDS);
  } else if (PGD->isBSAFile(DDSPath)) {
    spdlog::trace(L"Reading DDS BSA file {}", DDSPath.wstring());
    vector<std::byte> DDSBytes = PGD->getFile(DDSPath);

    // Load DDS file
    HR = DirectX::LoadFromDDSMemory(DDSBytes.data(), DDSBytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, DDS);
  } else {
    spdlog::trace(L"Reading DDS file from output dir {}", DDSPath.wstring());
    filesystem::path FullPath = OutputDir / DDSPath;

    // Load DDS file
    HR = DirectX::LoadFromDDSFile(FullPath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, DDS);
  }

  if (FAILED(HR)) {
    spdlog::error(L"Failed to load DDS file from {}: {}", DDSPath.wstring(),
                  strToWstr(getHRESULTErrorMessage(HR)));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenD3D::getDDSMetadata(const filesystem::path &DDSPath,
                                    DirectX::TexMetadata &DDSMeta) -> ParallaxGenTask::PGResult {
  // Use lock_guard to make this method thread-safe
  lock_guard<mutex> Lock(DDSMetaDataMutex);

  // Check if in cache
  // TODO set cache to something on failure
  if (DDSMetaDataCache.find(DDSPath) != DDSMetaDataCache.end()) {
    DDSMeta = DDSMetaDataCache[DDSPath];
    return ParallaxGenTask::PGResult::SUCCESS;
  }

  HRESULT HR{};

  if (PGD->isLooseFile(DDSPath)) {
    spdlog::trace(L"Reading DDS loose file metadata {}", DDSPath.wstring());
    filesystem::path FullPath = PGD->getFullPath(DDSPath);

    // Load DDS file
    HR = DirectX::GetMetadataFromDDSFile(FullPath.c_str(), DirectX::DDS_FLAGS_NONE, DDSMeta);
  } else if (PGD->isBSAFile(DDSPath)) {
    spdlog::trace(L"Reading DDS BSA file metadata {}", DDSPath.wstring());
    vector<std::byte> DDSBytes = PGD->getFile(DDSPath);

    // Load DDS file
    HR = DirectX::GetMetadataFromDDSMemory(DDSBytes.data(), DDSBytes.size(), DirectX::DDS_FLAGS_NONE, DDSMeta);
  } else {
    spdlog::trace(L"Reading DDS file from output dir {}", DDSPath.wstring());
    filesystem::path FullPath = OutputDir / DDSPath;

    // Load DDS file
    HR = DirectX::GetMetadataFromDDSFile(FullPath.c_str(), DirectX::DDS_FLAGS_NONE, DDSMeta);
  }

  if (FAILED(HR)) {
    spdlog::error(L"Failed to load DDS file metadata from {}: {}", DDSPath.wstring(),
                  strToWstr(getHRESULTErrorMessage(HR)));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // update cache
  DDSMetaDataCache[DDSPath] = DDSMeta;

  return ParallaxGenTask::PGResult::SUCCESS;
}

DirectX::ScratchImage ParallaxGenD3D::loadRawPixelsToScratchImage(const vector<unsigned char> &RawPixels,
                                                                  const size_t &Width, const size_t &Height,
                                                                  const size_t &Mips, DXGI_FORMAT Format) {
  // Initialize a ScratchImage
  DirectX::ScratchImage Image;
  HRESULT HR = Image.Initialize2D(Format, Width, Height, 1,
                                  Mips); // 1 array slice, 1 mipmap level
  if (FAILED(HR)) {
    spdlog::debug("Failed to initialize ScratchImage: {}", getHRESULTErrorMessage(HR));
    return {};
  }

  // Get the image data
  const DirectX::Image *Img = Image.GetImage(0, 0, 0);
  if (Img == nullptr) {
    spdlog::debug("Failed to get image data from ScratchImage");
    return {};
  }

  // Copy the raw pixel data into the image
  memcpy(Img->pixels, RawPixels.data(), RawPixels.size());

  return Image;
}

auto ParallaxGenD3D::getHRESULTErrorMessage(HRESULT HR) -> string {
  // Get error message
  _com_error Err(HR);
  return Err.ErrorMessage();
}
