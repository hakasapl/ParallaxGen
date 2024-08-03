#pragma once

#include <miniz.h>

#include <NifFile.hpp>
#include <array>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <tuple>

#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenTask.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherVanillaParallax.hpp"

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
  void patchMeshes();
  // zips all meshes and removes originals
  void zipMeshes() const;
  // deletes generated meshes
  void deleteMeshes() const;
  // deletes entire output folder
  void deleteOutputDir() const;
  // get state file name
  [[nodiscard]] static auto getStateFileName() -> std::filesystem::path;
  // init output folder
  void initOutputDir() const;

private:
  // upgrades a height map to complex material
  auto convertHeightMapToComplexMaterial(const std::filesystem::path &HeightMap) -> ParallaxGenTask::PGResult;

  // processes a NIF file (enable parallax if needed)
  auto processNIF(const std::filesystem::path &NIFFile, const std::vector<nlohmann::json> &TPBRConfigs,
                  const std::vector<int> &SlotSearchVP, const std::vector<int> &SlotSearchCM,
                  std::vector<std::wstring> &DynCubeBlocklist) -> ParallaxGenTask::PGResult;

  // processes a shape within a NIF file
  auto processShape(const std::vector<nlohmann::json> &TPBRConfigs, nifly::NifFile &NIF, nifly::NiShape *NIFShape,
                    PatcherVanillaParallax &PatchVP, PatcherComplexMaterial &PatchCM, PatcherTruePBR &PatchTPBR,
                    bool &NIFModified) -> ParallaxGenTask::PGResult;

  // Zip methods
  void addFileToZip(mz_zip_archive &Zip, const std::filesystem::path &FilePath,
                    const std::filesystem::path &ZipPath) const;

  void zipDirectory(const std::filesystem::path &DirPath, const std::filesystem::path &ZipPath) const;
};
