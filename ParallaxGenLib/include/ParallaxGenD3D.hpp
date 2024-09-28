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

#define NUM_GPU_THREADS 16
#define GPU_BUFFER_SIZE_MULTIPLE 16
#define MAX_CHANNEL_VALUE 255

class ParallaxGenD3D {
private:
  ParallaxGenDirectory *PGD;

  std::filesystem::path OutputDir;
  std::filesystem::path ExePath;
  bool UseGPU;

  // GPU objects
  Microsoft::WRL::ComPtr<ID3D11Device> PtrDevice;         // GPU device
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> PtrContext; // GPU context

  static inline const D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0; // DX11

  // Each shader gets some things defined here
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> ShaderMergeToComplexMaterial;
  struct ShaderMergeToComplexMaterialParams {
    DirectX::XMFLOAT2 EnvMapScalingFactor;
    DirectX::XMFLOAT2 ParallaxMapScalingFactor;
    BOOL EnvMapAvailable;
    BOOL ParallaxMapAvailable;
    UINT IntScalingFactor;
  };
  struct ShaderMergeToComplexMaterialOutputBuffer {
    UINT MinEnvValue;
    UINT MaxEnvValue;
    UINT MinParallaxValue;
    UINT MaxParallaxValue;
  };
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> ShaderCountAlphaValues;

  std::unordered_map<std::filesystem::path, DirectX::TexMetadata> DDSMetaDataCache;
  std::mutex DDSMetaDataMutex;

public:
  // Constructor
  ParallaxGenD3D(ParallaxGenDirectory *PGD, std::filesystem::path OutputDir, std::filesystem::path ExePath,
                 const bool &UseGPU);

  // Initialize GPU (also compiles shaders)
  void initGPU();

  // Check methods
  // files found in the bsa excludes are never CM maps, used for vanilla env masks
  auto findCMMaps(const std::set<std::wstring>& BSAExcludes) -> ParallaxGenTask::PGResult;

  static auto getNumChannelsByFormat(const DXGI_FORMAT &Format) -> int;

  // Attempt to upgrade vanilla parallax to complex material
  [[nodiscard]] auto upgradeToComplexMaterial(const std::filesystem::path &ParallaxMap,
                                              const std::filesystem::path &EnvMap) const -> DirectX::ScratchImage;

  // Checks if the aspect ratio of two DDS files match
  auto checkIfAspectRatioMatches(const std::filesystem::path &DDSPath1, const std::filesystem::path &DDSPath2,
                                 bool &CheckAspect) -> ParallaxGenTask::PGResult;

  // Gets the error message from an HRESULT for logging
  static auto getHRESULTErrorMessage(HRESULT HR) -> std::string;

private:
  auto checkIfCM(const std::filesystem::path &DDSPath, bool &Result) -> ParallaxGenTask::PGResult;
  auto countAlphaValuesGPU(const DirectX::ScratchImage &Image) -> int;
  static auto countAlphaValuesCPU(const DirectX::ScratchImage &Image, const bool &BCCompressed) -> int;

  // GPU functions
  void initShaders();

  auto compileShader(const std::filesystem::path &Filename,
                     Microsoft::WRL::ComPtr<ID3DBlob> &ShaderBlob) const -> ParallaxGenTask::PGResult;

  auto createComputeShader(const std::wstring &ShaderPath,
                           Microsoft::WRL::ComPtr<ID3D11ComputeShader> &ShaderDest) -> ParallaxGenTask::PGResult;

  // GPU Helpers
  auto createTexture2D(const DirectX::ScratchImage &Texture,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> &Dest) const -> ParallaxGenTask::PGResult;

  auto createTexture2D(Microsoft::WRL::ComPtr<ID3D11Texture2D> &ExistingTexture,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> &Dest) const -> ParallaxGenTask::PGResult;

  auto createTexture2D(D3D11_TEXTURE2D_DESC &Desc,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> &Dest) const -> ParallaxGenTask::PGResult;

  auto
  createShaderResourceView(const Microsoft::WRL::ComPtr<ID3D11Texture2D> &Texture,
                           Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> &Dest) const -> ParallaxGenTask::PGResult;

  auto
  createUnorderedAccessView(const Microsoft::WRL::ComPtr<ID3D11Texture2D> &Texture,
                            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> &Dest) const -> ParallaxGenTask::PGResult;

  auto
  createUnorderedAccessView(const Microsoft::WRL::ComPtr<ID3D11Resource> &GPUResource,
                            const D3D11_UNORDERED_ACCESS_VIEW_DESC &Desc,
                            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> &Dest) const -> ParallaxGenTask::PGResult;

  auto createBuffer(const void *Data, D3D11_BUFFER_DESC &Desc,
                    Microsoft::WRL::ComPtr<ID3D11Buffer> &Dest) const -> ParallaxGenTask::PGResult;

  auto createConstantBuffer(const void *Data, const UINT &Size,
                            Microsoft::WRL::ComPtr<ID3D11Buffer> &Dest) const -> ParallaxGenTask::PGResult;

  auto BlockingDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, // NOLINT
                        UINT ThreadGroupCountZ) const -> ParallaxGenTask::PGResult;

  [[nodiscard]] auto readBack(const Microsoft::WRL::ComPtr<ID3D11Texture2D> &GPUResource,
                              const int &Channels) const -> std::vector<unsigned char>;

  template <typename T>
  [[nodiscard]] auto readBack(const Microsoft::WRL::ComPtr<ID3D11Buffer> &GPUResource) const -> std::vector<T>;

  // Texture Helpers
  auto getDDS(const std::filesystem::path &DDSPath, DirectX::ScratchImage &DDS) const -> ParallaxGenTask::PGResult;

  auto getDDSMetadata(const std::filesystem::path &DDSPath, DirectX::TexMetadata &DDSMeta) -> ParallaxGenTask::PGResult;

  static auto loadRawPixelsToScratchImage(const std::vector<unsigned char> &RawPixels, const size_t &Width,
                                          const size_t &Height, const size_t &Mips, DXGI_FORMAT Format) -> DirectX::ScratchImage;

  static auto isPowerOfTwo(unsigned int X) -> bool;
};
