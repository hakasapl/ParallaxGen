#pragma once

#include <NifFile.hpp>
#include <cstdint>
#include <filesystem>
#include <miniz.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_set>

#include <boost/functional/hash.hpp>

#include "NIFUtil.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"
#include "patchers/PatcherUtil.hpp"

#define MESHES_LENGTH 7

class ParallaxGen {
private:
  std::filesystem::path OutputDir; // ParallaxGen output directory

  // Dependency objects
  ParallaxGenDirectory *PGD;
  ParallaxGenD3D *PGD3D;

  // sort blocks enabled, optimize disabled (for now)
  nifly::NifSaveOptions NIFSaveOptions = {false, false};

  struct ShapeKey {
    std::wstring NIFPath;
    int ShapeIndex;

    // Equality operator to compare two ShapeKey objects
    auto operator==(const ShapeKey &Other) const -> bool {
      return NIFPath == Other.NIFPath && ShapeIndex == Other.ShapeIndex;
    }
  };

  // Runner vars
  PatcherUtil::PatcherSet Patchers;
  std::unordered_map<std::wstring, int> *ModPriority;

  // Define a hash function for ShapeKey
  struct ShapeKeyHash {
    auto operator()(const ShapeKey &Key) const -> size_t {
      // Hash the NIFPath and ShapeIndex individually
      std::size_t H1 = std::hash<std::wstring>{}(Key.NIFPath);
      std::size_t H2 = std::hash<int>{}(Key.ShapeIndex);

      return H1 ^ (H2 << 1); // shifting to reduce collisions
    }
  };

  std::unordered_map<ShapeKey, std::vector<PatcherUtil::ShaderPatcherMatch>, ShapeKeyHash> AllowedShadersCache;
  std::mutex AllowedShadersCacheMutex;

public:
  //
  // The following methods are called from main.cpp and are public facing
  //

  // constructor
  ParallaxGen(std::filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D,
              const bool &OptimizeMeshes = false);
  void loadPatchers(const PatcherUtil::PatcherSet &Patchers);
  void loadModPriorityMap(std::unordered_map<std::wstring, int> *ModPriority);
  // enables parallax on relevant meshes
  void patchMeshes(const bool &MultiThread = true, const bool &PatchPlugin = true);
  // Dry run for finding potential matches (used with mod manager integration)
  [[nodiscard]] auto findModConflicts(const bool &MultiThread = true, const bool &PatchPlugin = true)
      -> std::unordered_map<std::wstring, std::tuple<std::set<NIFUtil::ShapeShader>, std::unordered_set<std::wstring>>>;
  // zips all meshes and removes originals
  void zipMeshes() const;
  // deletes entire output folder
  void deleteOutputDir(const bool &PreOutput = true) const;
  // get output zip name
  [[nodiscard]] static auto getOutputZipName() -> std::filesystem::path;
  // get diff json name
  [[nodiscard]] static auto getDiffJSONName() -> std::filesystem::path;

private:
  // Helper structs
  struct VectorHash {
    auto operator()(const std::vector<NIFUtil::ShapeShader> &Vec) const -> std::size_t {
      std::size_t Hash = 0;
      for (const auto &Shader : Vec) {
        boost::hash_combine(Hash, static_cast<int>(Shader));
      }
      return Hash;
    }
  };

  // thread safe JSON update
  std::mutex JSONUpdateMutex;
  void threadSafeJSONUpdate(const std::function<void(nlohmann::json &)> &Operation, nlohmann::json &DiffJSON);

  // processes a NIF file (enable parallax if needed)
  auto processNIF(const std::filesystem::path &NIFFile, nlohmann::json *DiffJSON, const bool &PatchPlugin = true,
                  PatcherUtil::ConflictModResults *ConflictMods = nullptr) -> ParallaxGenTask::PGResult;

  auto processNIF(const std::filesystem::path &NIFFile, const std::vector<std::byte> &NIFBytes, bool &NIFModified,
                  const std::vector<NIFUtil::ShapeShader> *ForceShaders = nullptr,
                  std::vector<std::pair<std::filesystem::path, nifly::NifFile>> *DupNIFs = nullptr,
                  const bool &PatchPlugin = true,
                  PatcherUtil::ConflictModResults *ConflictMods = nullptr) -> nifly::NifFile;

  // processes a shape within a NIF file
  auto processShape(const std::filesystem::path &NIFPath, nifly::NifFile &NIF, nifly::NiShape *NIFShape,
                    const int &ShapeIndex, PatcherUtil::PatcherObjectSet &Patchers, bool &ShapeModified,
                    NIFUtil::ShapeShader &ShaderApplied,
                    PatcherUtil::ConflictModResults *ConflictMods = nullptr,
                    const NIFUtil::ShapeShader *ForceShader = nullptr) -> ParallaxGenTask::PGResult;

  // Zip methods
  void addFileToZip(mz_zip_archive &Zip, const std::filesystem::path &FilePath,
                    const std::filesystem::path &ZipPath) const;

  void zipDirectory(const std::filesystem::path &DirPath, const std::filesystem::path &ZipPath) const;
};
