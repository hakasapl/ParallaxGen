#include "ParallaxGenDirectory.hpp"

#include <DirectXTex.h>
#include <NifFile.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <filesystem>
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

auto ParallaxGenDirectory::findPGFiles() -> void {
  mutex UnconfirmedTexturesMutex;
  unordered_map<filesystem::path, unordered_set<NIFUtil::TextureSlots>> UnconfirmedTextures;
  unordered_set<filesystem::path> UnconfirmedMeshes;

  spdlog::info("Finding textures and meshes");
  const auto& FileMap = getFileMap();
  for (const auto& [Path, File] : FileMap) {
    if (boost::iequals(Path.extension().wstring(), L".dds")) {
      // Found a DDS
      UnconfirmedTextures[Path] = {};
    } else if (boost::iequals(Path.extension().wstring(), L".nif")) {
      // Found a NIF
      UnconfirmedMeshes.insert(Path);
    } else if (boost::iequals(Path.extension().wstring(), L".json")) {
      // Found a JSON file
      const auto& FirstPath = Path.begin()->wstring();
      if (boost::iequals(FirstPath, L"pbrnifpatcher")) {
        // Found PBR JSON config
        PBRJSONs.push_back(Path);
      } else if (boost::iequals(FirstPath, L"parallaxgen")) {
        // Found ParallaxGen JSON config TODO
        PGJSONs.push_back(Path);
      }
    }
  }

  // Create task tracker
  ParallaxGenTask TaskTracker("Mapping Textures", UnconfirmedMeshes.size());

  // Create thread pool
  size_t NumThreads = boost::thread::hardware_concurrency();
  boost::asio::thread_pool MapTextureFromMeshPool(NumThreads);

  // TODO blacklisting
  // TODO config processing
  // TODO higher memory usage for better speed (probably move NIFUTIL nif loading method here)
  spdlog::info("Reading meshes to match textures to slots");
  // Loop through each mesh to confirm textures
  for (const auto& Mesh : UnconfirmedMeshes) {
    boost::asio::post(MapTextureFromMeshPool, [this, &TaskTracker, &Mesh, &UnconfirmedTextures, &UnconfirmedTexturesMutex] {
      TaskTracker.completeJob(mapTexturesFromNIF(Mesh, UnconfirmedTextures, UnconfirmedTexturesMutex));
    });
  }

  // TODO also check TXST sets here when mapping textures

  // Wait for all threads to complete
  MapTextureFromMeshPool.join();

  spdlog::info("Assigning textures to slots in texture map");
  // Loop through unconfirmed textures to confirm them
  for (const auto& [Texture, Slots] : UnconfirmedTextures) {
    if (Slots.size() == 1) {
      // Only one slot, confirm texture
      // Get base
      addToTextureMaps(Texture, *Slots.begin());
      continue;
    }

    // we need to find what it is by suffix, no NIF match
    const auto Slot = NIFUtil::getSlotFromPath(Texture);
    addToTextureMaps(Texture, Slot);
  }
}

auto ParallaxGenDirectory::mapTexturesFromNIF(const filesystem::path &NIFPath, unordered_map<filesystem::path, unordered_set<NIFUtil::TextureSlots>> &UnconfirmedTextures, mutex &UnconfirmedTexturesMutex) -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Load NIF
  const auto& NIFBytes = getFile(NIFPath);
  NifFile NIF;
  try {
    // Attempt to load NIF file
    NIF = NIFUtil::loadNIFFromBytes(NIFBytes);
  } catch(const exception& E) {
    // Unable to read NIF, delete from Meshes set
    spdlog::error(L"Error reading NIF File \"{}\" (skipping): {}", NIFPath.wstring(), stringToWstring(E.what()));
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // Loop through each shape
  bool HasAtLeastOneTextureSet = false;
  for (auto& Shape : NIF.GetShapes()) {
    if (!Shape->HasShaderProperty()) {
      // No shader, skip
      continue;
    }

    // Get shader
    const auto& Shader = NIF.GetShader(Shape);

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
      boost::to_lower(Texture);  // Lowercase for comparison

      if (Texture.empty()) {
        // No texture in this slot
        continue;
      }

      // Check if texture is confirmed
      const auto& TexturePath = filesystem::path(Texture);
      updateUnconfirmedTextureMap(TexturePath, static_cast<NIFUtil::TextureSlots>(Slot), UnconfirmedTextures, UnconfirmedTexturesMutex);
    }
  }

  if (HasAtLeastOneTextureSet) {
    // Add mesh to set
    addMesh(NIFPath);
  }

  return Result;
}

auto ParallaxGenDirectory::updateUnconfirmedTextureMap(const filesystem::path &Path, const NIFUtil::TextureSlots &Slot, unordered_map<filesystem::path, unordered_set<NIFUtil::TextureSlots>> &UnconfirmedTextures, mutex &Mutex) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(Mutex);

  // Check if texture is already in map
  if (UnconfirmedTextures.find(Path) != UnconfirmedTextures.end()) {
    // Texture is present
    UnconfirmedTextures[Path].insert(static_cast<NIFUtil::TextureSlots>(Slot));
  }
}

auto ParallaxGenDirectory::addToTextureMaps(const filesystem::path &Path, const NIFUtil::TextureSlots &Slot) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(TextureMapsMutex);

  // Get texture base
  const auto& Base = NIFUtil::getTexBase(Path.wstring());
  const auto& SlotInt = static_cast<size_t>(Slot);

  // Check if texture is already in map
  if (TextureMaps[SlotInt].find(Base) != TextureMaps[SlotInt].end()) {
    // Texture already exists
    spdlog::warn(L"Texture {} already exists in map in slot {}, replacing. This might cause issues.", Base, SlotInt);
  }

  // Add to texture map
  NIFUtil::PGTexture NewPGTexture = {Path, NIFUtil::getDefaultTextureType(Slot)};
  TextureMaps[SlotInt][Base] = NewPGTexture;
}

auto ParallaxGenDirectory::addMesh(const filesystem::path &Path) -> void {
  // Use mutex to make this thread safe
  lock_guard<mutex> Lock(MeshesMutex);

  // Add mesh to set
  Meshes.push_back(Path);
}

auto ParallaxGenDirectory::getTextureMap(const NIFUtil::TextureSlots &Slot) -> map<wstring, NIFUtil::PGTexture>& {
  return TextureMaps[static_cast<size_t>(Slot)];
}

auto ParallaxGenDirectory::getTextureMapConst(const NIFUtil::TextureSlots &Slot) const -> const map<wstring, NIFUtil::PGTexture>& {
  return TextureMaps[static_cast<size_t>(Slot)];
}

auto ParallaxGenDirectory::getMeshes() const -> const vector<filesystem::path>& {
  return Meshes;
}

auto ParallaxGenDirectory::getPBRJSONs() const -> const vector<filesystem::path>& {
  return PBRJSONs;
}

auto ParallaxGenDirectory::getPGJSONs() const -> const vector<filesystem::path>& {
  return PGJSONs;
}
