#pragma once

#include <NifFile.hpp>
#include <filesystem>
#include <miniz.h>
#include <mutex>
#include <nlohmann/json.hpp>

#include "NIFUtil.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherVanillaParallax.hpp"

#define MESHES_LENGTH 7

class ParallaxGen {
private:
  std::filesystem::path OutputDir; // ParallaxGen output directory

  // Dependency objects
  ParallaxGenDirectory *PGD;
  ParallaxGenConfig *PGC;
  ParallaxGenD3D *PGD3D;

  // sort blocks enabled, optimize disabled (for now)
  nifly::NifSaveOptions NIFSaveOptions = {false, true};

  // store CLI arguments
  bool IgnoreParallax;
  bool IgnoreCM;
  bool IgnoreTruePBR;

public:
  //
  // The following methods are called from main.cpp and are public facing
  //

  // constructor
  ParallaxGen(std::filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D,
              const bool &OptimizeMeshes = false, const bool &IgnoreParallax = false, const bool &IgnoreCM = false,
              const bool &IgnoreTruePBR = false);
  // upgrades textures whenever possible
  void upgradeShaders();
  // enables parallax on relevant meshes
  void patchMeshes(const bool &MultiThread = true);
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
  auto convertHeightMapToComplexMaterial(const std::filesystem::path &HeightMap) -> ParallaxGenTask::PGResult;

  // processes a NIF file (enable parallax if needed)
  auto processNIF(const std::filesystem::path &NIFFile, nlohmann::json &DiffJSON) -> ParallaxGenTask::PGResult;

  // processes a shape within a NIF file
  auto processShape(const std::filesystem::path &NIFPath, nifly::NifFile &NIF, nifly::NiShape *NIFShape,
                    PatcherVanillaParallax &PatchVP, PatcherComplexMaterial &PatchCM, PatcherTruePBR &PatchTPBR,
                    bool &ShapeModified, bool &ShapeDeleted, NIFUtil::ShapeShader &ShaderApplied) const -> ParallaxGenTask::PGResult;

  // Zip methods
  void addFileToZip(mz_zip_archive &Zip, const std::filesystem::path &FilePath,
                    const std::filesystem::path &ZipPath) const;

  void zipDirectory(const std::filesystem::path &DirPath, const std::filesystem::path &ZipPath) const;
};
