#include "ParallaxGenDirectory.hpp"

#include <DirectXTex.h>
#include <NifFile.hpp>
#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <filesystem>
#include <mutex>
#include <shlwapi.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <winnt.h>

#include "BethesdaDirectory.hpp"
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame BG, filesystem::path OutputPath, ModManagerDirectory *MMD)
    : BethesdaDirectory(BG, std::move(OutputPath), MMD, true) {}

auto ParallaxGenDirectory::findFiles() -> void {
  // Clear existing unconfirmedtextures
  UnconfirmedTextures.clear();
  UnconfirmedMeshes.clear();

  // Populate unconfirmed maps
  spdlog::info("Finding Relevant Files");
  const auto &FileMap = getFileMap();

  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  for (const auto &[Path, File] : FileMap) {
    const auto &FirstPath = Path.begin()->wstring();
    if (boost::iequals(FirstPath, "textures") && boost::iequals(Path.extension().wstring(), L".dds")) {
      if (!isPathAscii(Path)) {
        // Skip non-ascii paths
        spdlog::warn(L"Texture {} contains non-ascii characters which are not allowed - skipping", Path.wstring());
        continue;
      }

      // Found a DDS
      spdlog::trace(L"Finding Files | Found DDS | {}", Path.wstring());
      UnconfirmedTextures[Path] = {};
    } else if (boost::iequals(FirstPath, "meshes") && boost::iequals(Path.extension().wstring(), L".nif")) {
      // Found a NIF
      spdlog::trace(L"Finding Files | Found NIF | {}", Path.wstring());
      UnconfirmedMeshes.insert(Path);
    } else if (boost::iequals(Path.extension().wstring(), L".json")) {
      // Found a JSON file
      if (boost::iequals(FirstPath, L"pbrnifpatcher")) {
        // Found PBR JSON config
        spdlog::trace(L"Finding Files | Found PBR JSON | {}", Path.wstring());
        PBRJSONs.push_back(Path);
      } else if (boost::iequals(FirstPath, L"parallaxgen")) {
        // Found ParallaxGen JSON config TODO
        spdlog::trace(L"Finding Files | Found ParallaxGen JSON | {}", Path.wstring());
        PGJSONs.push_back(Path);
      }
    }
  }
  spdlog::info("Finding files done");
}

auto ParallaxGenDirectory::mapFiles(const vector<wstring> &NIFBlocklist, const vector<wstring> &NIFAllowlist,
                                    const vector<pair<wstring, NIFUtil::TextureType>> &ManualTextureMaps,
                                    const vector<wstring> &ParallaxBSAExcludes, const bool &MapFromMeshes,
                                    const bool &Multithreading, const bool &CacheNIFs) -> void {
  findFiles();

  // Helpers
  const unordered_map<wstring, NIFUtil::TextureType> ManualTextureMapsMap(ManualTextureMaps.begin(),
                                                                          ManualTextureMaps.end());

  spdlog::info("Starting building texture map");

  // Create task tracker
  ParallaxGenTask TaskTracker("Loading NIFs", UnconfirmedMeshes.size(), MAPTEXTURE_PROGRESS_MODULO);

  // Create thread pool
#ifdef _DEBUG
  size_t NumThreads = 1;
#else
  const size_t NumThreads = boost::thread::hardware_concurrency();
#endif

  atomic<bool> KillThreads = false;

  boost::asio::thread_pool MapTextureFromMeshPool(NumThreads);

  // Loop through each mesh to confirm textures
  for (const auto &Mesh : UnconfirmedMeshes) {
    if (!NIFAllowlist.empty() && !checkGlobMatchInVector(Mesh.wstring(), NIFAllowlist)) {
      // Skip mesh because it is not on allowlist
      spdlog::trace(L"Loading NIFs | Skipping Mesh due to Allowlist | Mesh: {}", Mesh.wstring());
      TaskTracker.completeJob(ParallaxGenTask::PGResult::SUCCESS);
      continue;
    }

    if (!NIFBlocklist.empty() && checkGlobMatchInVector(Mesh.wstring(), NIFBlocklist)) {
      // Skip mesh because it is on blocklist
      spdlog::trace(L"Loading NIFs | Skipping Mesh due to Blocklist | Mesh: {}", Mesh.wstring());
      TaskTracker.completeJob(ParallaxGenTask::PGResult::SUCCESS);
      continue;
    }

    if (!MapFromMeshes) {
      // Skip mapping textures from meshes
      Meshes.insert(Mesh);
      TaskTracker.completeJob(ParallaxGenTask::PGResult::SUCCESS);
      continue;
    }

    if (Multithreading) {
      boost::asio::post(MapTextureFromMeshPool, [this, &TaskTracker, &Mesh, &CacheNIFs, &KillThreads] {
        ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
        try {
          Result = mapTexturesFromNIF(Mesh, CacheNIFs);
        } catch (const exception &E) {
          spdlog::error(L"Exception in thread loading NIF \"{}\": {}", Mesh.wstring(), ASCIItoUTF16(E.what()));
          KillThreads.store(true, std::memory_order_release);
          Result = ParallaxGenTask::PGResult::FAILURE;
        }

        TaskTracker.completeJob(Result);
      });
    } else {
      TaskTracker.completeJob(mapTexturesFromNIF(Mesh, CacheNIFs));
    }
  }

  if (Multithreading) {
    while (!TaskTracker.isCompleted()) {
      if (KillThreads.load(std::memory_order_acquire)) {
        MapTextureFromMeshPool.stop();
        throw runtime_error("Exception in thread mapping textures from NIFs");
      }

      this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for all threads to complete
    MapTextureFromMeshPool.join();
  }

  // Loop through unconfirmed textures to confirm them
  for (const auto &[Texture, Property] : UnconfirmedTextures) {
    bool FoundInstance = false;

    // Find winning texture slot
    size_t MaxVal = 0;
    NIFUtil::TextureSlots WinningSlot = {};
    for (const auto &[Slot, Count] : Property.Slots) {
      FoundInstance = true;
      if (Count > MaxVal) {
        MaxVal = Count;
        WinningSlot = Slot;
      }
    }

    // Find winning texture type
    MaxVal = 0;
    NIFUtil::TextureType WinningType = {};
    for (const auto &[Type, Count] : Property.Types) {
      FoundInstance = true;
      if (Count > MaxVal) {
        MaxVal = Count;
        WinningType = Type;
      }
    }

    if (!FoundInstance) {
      // Determine slot and type by suffix
      const auto DefProperty = NIFUtil::getDefaultsFromSuffix(Texture);
      WinningSlot = get<0>(DefProperty);
      WinningType = get<1>(DefProperty);
    }

    if (ManualTextureMapsMap.find(Texture.wstring()) != ManualTextureMapsMap.end()) {
      // Manual texture map found, override
      WinningType = ManualTextureMapsMap.at(Texture.wstring());
      WinningSlot = NIFUtil::getSlotFromTexType(WinningType);
    }

    if ((WinningSlot == NIFUtil::TextureSlots::PARALLAX) && isFileInBSA(Texture, ParallaxBSAExcludes)) {
      spdlog::trace(L"Mapping Textures | Ignored vanilla parallax texture | Texture: {}", Texture.wstring());
      continue;
    }

    // Log result
    spdlog::trace(L"Mapping Textures | Mapping Result | Texture: {} | Slot: {} | Type: {}", Texture.wstring(),
                  static_cast<size_t>(WinningSlot), UTF8toUTF16(NIFUtil::getStrFromTexType(WinningType)));

    // Add to texture map
    if (WinningSlot != NIFUtil::TextureSlots::UNKNOWN) {
      // Only add if no unknowns
      addToTextureMaps(Texture, WinningSlot, WinningType);
    }
  }

  // cleanup
  UnconfirmedTextures.clear();
  UnconfirmedMeshes.clear();

  spdlog::info("Mapping textures done");
}

auto ParallaxGenDirectory::checkGlobMatchInVector(const wstring &Check, const vector<std::wstring> &List) -> bool {
  // convert wstring to LPCWSTR
  LPCWSTR CheckCstr = Check.c_str();

  // check if string matches any glob
  return std::ranges::any_of(List, [&](const wstring &Glob) { return PathMatchSpecW(CheckCstr, Glob.c_str()); });
}

auto ParallaxGenDirectory::mapTexturesFromNIF(const filesystem::path &NIFPath,
                                              const bool &CacheNIFs) -> ParallaxGenTask::PGResult {
  if (boost::icontains(NIFPath.wstring(), "opheliamarryringsmall")) {
    // Skip LOD meshes
    spdlog::trace("DEBUG");
  }

  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Load NIF
  vector<std::byte> NIFBytes;
  try {
    NIFBytes = getFile(NIFPath, CacheNIFs);
  } catch (const exception &E) {
    spdlog::error(L"Error reading NIF File \"{}\" (skipping): {}", NIFPath.wstring(), ASCIItoUTF16(E.what()));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  NifFile NIF;
  try {
    // Attempt to load NIF file
    NIF = NIFUtil::loadNIFFromBytes(NIFBytes);
  } catch (const exception &E) {
    // Unable to read NIF, delete from Meshes set
    spdlog::error(L"Error reading NIF File \"{}\" (skipping): {}", NIFPath.wstring(), ASCIItoUTF16(E.what()));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // Loop through each shape
  bool HasAtLeastOneTextureSet = false;
  for (auto &Shape : NIF.GetShapes()) {
    if (!Shape->HasShaderProperty()) {
      // No shader, skip
      continue;
    }

    // Get shader
    auto *const Shader = NIF.GetShader(Shape);
    if (Shader == nullptr) {
      // No shader, skip
      continue;
    }

    if (!Shader->HasTextureSet()) {
      // No texture set, skip
      continue;
    }

    // We have a texture set
    HasAtLeastOneTextureSet = true;

    // Loop through each texture slot
    for (uint32_t Slot = 0; Slot < NUM_TEXTURE_SLOTS; Slot++) {
      string Texture;
      NIF.GetTextureSlot(Shape, Texture, Slot);

      if (!ContainsOnlyAscii(Texture)) {
        spdlog::error(L"NIF {} has texture slot(s) with invalid non-ASCII chars (skipping)", NIFPath.wstring());
        return ParallaxGenTask::PGResult::FAILURE;
      }

      if (Texture.empty()) {
        // No texture in this slot
        continue;
      }

      boost::to_lower(Texture); // Lowercase for comparison

      const auto ShaderType = Shader->GetShaderType();
      NIFUtil::TextureType TextureType = {};

      // Check to make sure appropriate shaders are set for a given texture
      auto *const ShaderBSSP = dynamic_cast<BSShaderProperty *>(Shader);
      if (ShaderBSSP == nullptr) {
        // Not a BSShaderProperty, skip
        continue;
      }

      switch (static_cast<NIFUtil::TextureSlots>(Slot)) {
      case NIFUtil::TextureSlots::DIFFUSE:
        // Diffuse check
        TextureType = NIFUtil::TextureType::DIFFUSE;
        break;
      case NIFUtil::TextureSlots::NORMAL:
        // Normal check
        if (ShaderType == BSLSP_SKINTINT && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF1_FACEGEN_RGB_TINT)) {
          // This is a skin tint map
          TextureType = NIFUtil::TextureType::MODELSPACENORMAL;
          break;
        }

        TextureType = NIFUtil::TextureType::NORMAL;
        break;
      case NIFUtil::TextureSlots::GLOW:
        // Glowmap check
        if ((ShaderType == BSLSP_GLOWMAP && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_GLOW_MAP)) ||
            (ShaderType == BSLSP_DEFAULT && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_UNUSED01))) {
          // This is an emmissive map (either vanilla glowmap shader or PBR)
          TextureType = NIFUtil::TextureType::EMISSIVE;
          break;
        }

        if (ShaderType == BSLSP_MULTILAYERPARALLAX && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_MULTI_LAYER_PARALLAX)) {
          // This is a subsurface map
          TextureType = NIFUtil::TextureType::SUBSURFACECOLOR;
          break;
        }

        if (ShaderType == BSLSP_SKINTINT && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF1_FACEGEN_RGB_TINT)) {
          // This is a skin tint map
          TextureType = NIFUtil::TextureType::SKINTINT;
          break;
        }

        continue;
      case NIFUtil::TextureSlots::PARALLAX:
        // Parallax check
        if ((ShaderType == BSLSP_PARALLAX && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF1_PARALLAX)) ||
            (ShaderType == BSLSP_DEFAULT && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_UNUSED01))) {
          // This is a height map
          TextureType = NIFUtil::TextureType::HEIGHT;
          break;
        }

        continue;
      case NIFUtil::TextureSlots::CUBEMAP:
        // Cubemap check
        if (ShaderType == BSLSP_ENVMAP && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF1_ENVIRONMENT_MAPPING)) {
          TextureType = NIFUtil::TextureType::CUBEMAP;
          break;
        }

        continue;
      case NIFUtil::TextureSlots::ENVMASK:
        // Envmap check
        if (ShaderType == BSLSP_ENVMAP && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF1_ENVIRONMENT_MAPPING)) {
          TextureType = NIFUtil::TextureType::ENVIRONMENTMASK;
          break;
        }

        if (ShaderType == BSLSP_DEFAULT && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_UNUSED01)) {
          TextureType = NIFUtil::TextureType::RMAOS;
          break;
        }

        continue;
      case NIFUtil::TextureSlots::MULTILAYER:
        // Tint check
        if (ShaderType == BSLSP_MULTILAYERPARALLAX && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_MULTI_LAYER_PARALLAX)) {
          if (NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_UNUSED01)) {
            // 2 layer PBR
            TextureType = NIFUtil::TextureType::COATNORMALROUGHNESS;
          } else {
            // normal multilayer
            TextureType = NIFUtil::TextureType::INNERLAYER;
          }
          break;
        }

        continue;
      case NIFUtil::TextureSlots::BACKLIGHT:
        // Backlight check
        if (ShaderType == BSLSP_MULTILAYERPARALLAX && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_UNUSED01)) {
          TextureType = NIFUtil::TextureType::SUBSURFACEPBR;
          break;
        }

        if (NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_BACK_LIGHTING)) {
          TextureType = NIFUtil::TextureType::BACKLIGHT;
          break;
        }

        if (ShaderType == BSLSP_SKINTINT && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF1_FACEGEN_RGB_TINT)) {
          TextureType = NIFUtil::TextureType::SPECULAR;
          break;
        }

        continue;
      default:
        TextureType = NIFUtil::TextureType::UNKNOWN;
      }

      // Log finding
      spdlog::trace(L"Mapping Textures | Slot Found | NIF: {} | Texture: {} | Slot: {} | Type: {}", NIFPath.wstring(),
                    ASCIItoUTF16(Texture), Slot, UTF8toUTF16(NIFUtil::getStrFromTexType(TextureType)));

      // Update unconfirmed textures map
      updateUnconfirmedTexturesMap(Texture, static_cast<NIFUtil::TextureSlots>(Slot), TextureType, UnconfirmedTextures);
    }
  }

  if (HasAtLeastOneTextureSet) {
    // Add mesh to set
    addMesh(NIFPath);
  }

  return Result;
}

auto ParallaxGenDirectory::updateUnconfirmedTexturesMap(
    const filesystem::path &Path, const NIFUtil::TextureSlots &Slot, const NIFUtil::TextureType &Type,
    unordered_map<filesystem::path, UnconfirmedTextureProperty> &UnconfirmedTextures) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(UnconfirmedTexturesMutex);

  // Check if texture is already in map
  if (UnconfirmedTextures.find(Path) != UnconfirmedTextures.end()) {
    // Texture is present
    UnconfirmedTextures[Path].Slots[Slot]++;
    UnconfirmedTextures[Path].Types[Type]++;
  }
}

auto ParallaxGenDirectory::addToTextureMaps(const filesystem::path &Path, const NIFUtil::TextureSlots &Slot,
                                            const NIFUtil::TextureType &Type) -> void {
  // Get texture base
  const auto &Base = NIFUtil::getTexBase(Path);
  const auto &SlotInt = static_cast<size_t>(Slot);

  // Add to texture map
  NIFUtil::PGTexture NewPGTexture = {Path, Type};
  {
    lock_guard<mutex> Lock(TextureMapsMutex);
    TextureMaps[SlotInt][Base].insert(NewPGTexture);
  }

  {
    lock_guard<mutex> Lock(TextureTypesMutex);
    TextureTypes[Path] = Type;
  }
}

auto ParallaxGenDirectory::addMesh(const filesystem::path &Path) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(MeshesMutex);

  // Add mesh to set
  Meshes.insert(Path);
}

auto ParallaxGenDirectory::getTextureMap(const NIFUtil::TextureSlots &Slot)
    -> map<wstring, unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> & {
  return TextureMaps[static_cast<size_t>(Slot)];
}

auto ParallaxGenDirectory::getTextureMapConst(const NIFUtil::TextureSlots &Slot) const
    -> const map<wstring, unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> & {
  return TextureMaps[static_cast<size_t>(Slot)];
}

auto ParallaxGenDirectory::getMeshes() const -> const unordered_set<filesystem::path> & { return Meshes; }

auto ParallaxGenDirectory::getPBRJSONs() const -> const vector<filesystem::path> & { return PBRJSONs; }

auto ParallaxGenDirectory::getPGJSONs() const -> const vector<filesystem::path> & { return PGJSONs; }

void ParallaxGenDirectory::setTextureType(const filesystem::path &Path, const NIFUtil::TextureType &Type) {
  lock_guard<mutex> Lock(TextureTypesMutex);

  TextureTypes[Path] = Type;
}

auto ParallaxGenDirectory::getTextureType(const filesystem::path &Path) -> NIFUtil::TextureType {
  lock_guard<mutex> Lock(TextureTypesMutex);

  if (TextureTypes.find(Path) != TextureTypes.end()) {
    return TextureTypes.at(Path);
  }

  return NIFUtil::TextureType::UNKNOWN;
}
