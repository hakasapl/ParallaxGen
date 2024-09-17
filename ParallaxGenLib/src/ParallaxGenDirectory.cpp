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
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "NIFUtil.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame BG) : BethesdaDirectory(BG, true) {}

auto ParallaxGenDirectory::findPGFiles(const bool &MapFromMeshes) -> void {
  mutex UnconfirmedTexturesMutex;
  unordered_map<filesystem::path, UnconfirmedTextureProperty> UnconfirmedTextures;
  unordered_set<filesystem::path> UnconfirmedMeshes;

  spdlog::info("Starting building texture map");

  // Populate unconfirmed maps
  spdlog::info("Finding relevant files");
  const auto &FileMap = getFileMap();
  for (const auto &[Path, File] : FileMap) {
    if (boost::iequals(Path.extension().wstring(), L".dds")) {
      // Found a DDS
      UnconfirmedTextures[Path] = {};
    } else if (boost::iequals(Path.extension().wstring(), L".nif")) {
      // Found a NIF
      UnconfirmedMeshes.insert(Path);
    } else if (boost::iequals(Path.extension().wstring(), L".json")) {
      // Found a JSON file
      const auto &FirstPath = Path.begin()->wstring();
      if (boost::iequals(FirstPath, L"pbrnifpatcher")) {
        // Found PBR JSON config
        PBRJSONs.push_back(Path);
      } else if (boost::iequals(FirstPath, L"parallaxgen")) {
        // Found ParallaxGen JSON config TODO
        PGJSONs.push_back(Path);
      }
    }
  }

  // TODO blacklisting
  // TODO config processing
  // TODO probably not all of this is needed depending on args provided
  if (MapFromMeshes) {
    // Create task tracker
    ParallaxGenTask TaskTracker("Mapping Textures", UnconfirmedMeshes.size(), 10);

    // Create thread pool
    size_t NumThreads = boost::thread::hardware_concurrency();
    boost::asio::thread_pool MapTextureFromMeshPool(NumThreads);

    spdlog::info("Reading meshes to match textures to slots");
    // Loop through each mesh to confirm textures
    for (const auto &Mesh : UnconfirmedMeshes) {
      boost::asio::post(
          MapTextureFromMeshPool, [this, &TaskTracker, &Mesh, &UnconfirmedTextures, &UnconfirmedTexturesMutex] {
            TaskTracker.completeJob(mapTexturesFromNIF(Mesh, UnconfirmedTextures, UnconfirmedTexturesMutex));
          });
    }

    // Wait for all threads to complete
    MapTextureFromMeshPool.join();

    // TODO also check TXST sets here when mapping textures
  } else {
    // Confirm everything because there is no way to check
    Meshes.insert(UnconfirmedMeshes.begin(), UnconfirmedMeshes.end());
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

    // Log result
    spdlog::trace(L"Mapping Textures | Mapping Result | Texture: {} | Slot: {} | Type: {}", Texture.wstring(),
                  static_cast<size_t>(WinningSlot), stringToWstring(NIFUtil::getTextureTypeStr(WinningType)));

    // Add to texture map
    addToTextureMaps(Texture, WinningSlot, WinningType);
  }
}

auto ParallaxGenDirectory::mapTexturesFromNIF(
    const filesystem::path &NIFPath, unordered_map<filesystem::path, UnconfirmedTextureProperty> &UnconfirmedTextures,
    mutex &UnconfirmedTexturesMutex) -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Load NIF
  const auto &NIFBytes = getFile(NIFPath);
  NifFile NIF;
  try {
    // Attempt to load NIF file
    NIF = NIFUtil::loadNIFFromBytes(NIFBytes);
  } catch (const exception &E) {
    // Unable to read NIF, delete from Meshes set
    spdlog::error(L"Error reading NIF File \"{}\" (skipping): {}", NIFPath.wstring(), stringToWstring(E.what()));
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
    const auto &Shader = NIF.GetShader(Shape);

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
      boost::to_lower(Texture); // Lowercase for comparison

      if (Texture.empty()) {
        // No texture in this slot
        continue;
      }

      const auto ShaderType = Shader->GetShaderType();
      NIFUtil::TextureType TextureType = {};

      // Check to make sure appropriate shaders are set for a given texture
      auto *const ShaderBSSP = dynamic_cast<BSShaderProperty *>(Shader);
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
          TextureType = NIFUtil::TextureType::SUBSURFACE;
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
        if ((ShaderType == BSLSP_DEFAULT && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF1_PARALLAX)) ||
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
      case NIFUtil::TextureSlots::TINT:
        // Tint check
        if (ShaderType == BSLSP_MULTILAYERPARALLAX && NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_MULTI_LAYER_PARALLAX)) {
          if (NIFUtil::hasShaderFlag(ShaderBSSP, SLSF2_UNUSED01)) {
            // 2 layer PBR
            TextureType = NIFUtil::TextureType::COATNORMAL;
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
          // TODO verify this
          TextureType = NIFUtil::TextureType::SUBSURFACE;
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
        TextureType = NIFUtil::TextureType::DIFFUSE;
      }

      // Log finding
      spdlog::trace(L"Mapping Textures | Slot Found | NIF: {} | Texture: {} | Slot: {} | Type: {}", NIFPath.wstring(),
                    stringToWstring(Texture), Slot, stringToWstring(NIFUtil::getTextureTypeStr(TextureType)));

      // Update unconfirmed textures map
      updateUnconfirmedTexturesMap(Texture, static_cast<NIFUtil::TextureSlots>(Slot), TextureType, UnconfirmedTextures,
                                   UnconfirmedTexturesMutex);
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
    unordered_map<filesystem::path, UnconfirmedTextureProperty> &UnconfirmedTextures, mutex &Mutex) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(Mutex);

  // Check if texture is already in map
  if (UnconfirmedTextures.find(Path) != UnconfirmedTextures.end()) {
    // Texture is present
    UnconfirmedTextures[Path].Slots[Slot]++;
    UnconfirmedTextures[Path].Types[Type]++;
  }
}

auto ParallaxGenDirectory::addToTextureMaps(const filesystem::path &Path, const NIFUtil::TextureSlots &Slot,
                                            const NIFUtil::TextureType &Type) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(TextureMapsMutex);

  // Get texture base
  const auto &Base = NIFUtil::getTexBase(Path);
  const auto &SlotInt = static_cast<size_t>(Slot);

  // Check if texture is already in map
  if (TextureMaps[SlotInt].find(Base) != TextureMaps[SlotInt].end()) {
    // Texture already exists
    spdlog::warn(L"Texture {} already exists in map in slot {}, replacing. This might cause issues.", Base, SlotInt);
  }

  // Add to texture map
  NIFUtil::PGTexture NewPGTexture = {Path, Type};
  TextureMaps[SlotInt][Base] = NewPGTexture;
}

auto ParallaxGenDirectory::addMesh(const filesystem::path &Path) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(MeshesMutex);

  // Add mesh to set
  Meshes.insert(Path);
}

auto ParallaxGenDirectory::getTextureMap(const NIFUtil::TextureSlots &Slot) -> map<wstring, NIFUtil::PGTexture> & {
  return TextureMaps[static_cast<size_t>(Slot)];
}

auto ParallaxGenDirectory::getTextureMapConst(const NIFUtil::TextureSlots &Slot) const
    -> const map<wstring, NIFUtil::PGTexture> & {
  return TextureMaps[static_cast<size_t>(Slot)];
}

auto ParallaxGenDirectory::getMeshes() const -> const unordered_set<filesystem::path> & { return Meshes; }

auto ParallaxGenDirectory::getPBRJSONs() const -> const vector<filesystem::path> & { return PBRJSONs; }

auto ParallaxGenDirectory::getPGJSONs() const -> const vector<filesystem::path> & { return PGJSONs; }
