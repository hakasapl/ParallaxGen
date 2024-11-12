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

  std::unordered_map<std::filesystem::path, DirectX::TexMetadata> DDSMetaDataCache{};
  std::mutex DDSMetaDataMutex;

  std::mutex UpgradeCMMutex;

public:
  /// @brief Constructor
  /// @param PGD ParallaxGenDirectory, must be populated before calling member functions
  /// @param OutputDir absolute path of output directory
  /// @param ExePath absolute path of executable directory
  /// @param UseGPU if the GPU should be used, currently only findCMMaps has a corresponding CPU implementation
  ParallaxGenD3D(ParallaxGenDirectory *PGD, std::filesystem::path OutputDir, std::filesystem::path ExePath,
                 const bool &UseGPU);

  /// @brief Initialize GPU (also compiles shaders)
  void initGPU();

  /// @brief Find complex material maps and re-assign the type in the used ParallaxGenDirectory
  /// @param[in] BSAExcludes never assume files found in these BSAs are complex material maps
  /// @return result of the operation
  auto findCMMaps(const std::unordered_set<std::wstring>& BSAExcludes) -> ParallaxGenTask::PGResult;

  /// @brief Create a complex material map from the given textures
  /// @param ParallaxMap relative path to the height map in the data directory
  /// @param EnvMap relative path to the env map mask in the data directory
  /// @return DirectX scratch image, format DXGI_FORMAT_BC3_UNORM on success, otherwise empty ScratchImage
  [[nodiscard]] auto upgradeToComplexMaterial(const std::filesystem::path &ParallaxMap,
                                              const std::filesystem::path &EnvMap) -> DirectX::ScratchImage;


  /// @brief Checks if the aspect ratio of two DDS files match
  /// @param[in] DDSPath1 relative path of the first file in the data directory
  /// @param[in] DDSPath2 relative path of the second file in the data directory
  /// @return if the aspect ratio matches
  auto checkIfAspectRatioMatches(const std::filesystem::path &DDSPath1, const std::filesystem::path &DDSPath2) -> bool;

  /// @brief  Gets the error message from an HRESULT for logging
  /// @param HR the HRESULT to check
  /// @return error message
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
