#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <miniz.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUI.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherVanillaParallax.hpp"

#define MESHES_LENGTH 7

class ParallaxGen {
private:
  std::filesystem::path OutputDir; // ParallaxGen output directory

  // Dependency objects
  ModManagerDirectory *MMD;
  ParallaxGenDirectory *PGD;
  ParallaxGenConfig *PGC;
  ParallaxGenD3D *PGD3D;

  // sort blocks enabled, optimize disabled (for now)
  nifly::NifSaveOptions NIFSaveOptions = {false, true};

  // store CLI arguments
  bool IgnoreParallax;
  bool IgnoreCM;
  bool IgnoreTruePBR;
  bool UpgradeShaders;
  bool MMEnabled;

  struct PatcherResult {
    std::wstring MatchedPath;
    bool ShouldApply;
  };

  // store diffuse mismatch messages
  struct PairHash {
    auto operator()(const std::pair<std::wstring, std::wstring> &P) const -> size_t {
      std::hash<std::wstring> HashWstr;
      return HashWstr(P.first) ^ (HashWstr(P.second) << 1);
    }
  };

  std::unordered_set<std::pair<std::wstring, std::wstring>, PairHash> MismatchTracker;

  // Mutex lock for TUI prompts
  std::mutex PromptMutex;

  // Mutex lock for shader upgrades
  std::mutex UpgradeMutex;

public:
  //
  // The following methods are called from main.cpp and are public facing
  //

  // constructor
  ParallaxGen(std::filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D,
              ModManagerDirectory *MMD, const bool &OptimizeMeshes = false,
              const bool &IgnoreParallax = false, const bool &IgnoreCM = false, const bool &IgnoreTruePBR = false,
              const bool &UpgradeShaders = false, const bool &MMEnabled = false);
  // enables parallax on relevant meshes
  void patchMeshes(const bool &MultiThread = true, const bool &PatchPlugin = true);
  // zips all meshes and removes originals
  void zipMeshes() const;
  // deletes generated meshes
  void deleteMeshes() const;
  // deletes entire output folder
  void deleteOutputDir() const;
  // get output zip name
  [[nodiscard]] static auto getOutputZipName() -> std::filesystem::path;
  // get diff json name
  [[nodiscard]] static auto getDiffJSONName() -> std::filesystem::path;

private:
  // thread safe JSON update
  std::mutex JSONUpdateMutex;
  void threadSafeJSONUpdate(const std::function<void(nlohmann::json &)> &Operation, nlohmann::json &DiffJSON);

  // upgrades a height map to complex material
  auto convertHeightMapToComplexMaterial(const std::filesystem::path &HeightMap,
                                         std::wstring &NewCMMap) -> ParallaxGenTask::PGResult;

  // processes a NIF file (enable parallax if needed)
  auto processNIF(const std::filesystem::path &NIFFile, nlohmann::json &DiffJSON,
                  const bool &PatchPlugin = true) -> ParallaxGenTask::PGResult;

  // processes a shape within a NIF file
  auto processShape(const std::filesystem::path &NIFPath, nifly::NifFile &NIF, nifly::NiShape *NIFShape,
                    PatcherVanillaParallax &PatchVP, PatcherComplexMaterial &PatchCM, PatcherTruePBR &PatchTPBR,
                    bool &ShapeModified, bool &ShapeDeleted,
                    NIFUtil::ShapeShader &ShaderApplied) -> ParallaxGenTask::PGResult;

  void findModMismatch(const std::wstring &BaseFile, const std::wstring &MatchedMod);

  auto promptForSelection(const std::unordered_map<std::wstring, std::pair<NIFUtil::ShapeShader, std::wstring>> &Mods)
      -> std::wstring;

  // Zip methods
  void addFileToZip(mz_zip_archive &Zip, const std::filesystem::path &FilePath,
                    const std::filesystem::path &ZipPath) const;

  void zipDirectory(const std::filesystem::path &DirPath, const std::filesystem::path &ZipPath) const;
};
