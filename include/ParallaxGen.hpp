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

#define NUM_TEXTURE_SLOTS 9

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
  enum class TextureSlots : unsigned int {
    Diffuse = 0,
    Normal = 1,
    Glow = 2,
    Parallax = 3,
    Cubemap = 4,
    EnvMask = 5,
    Tint = 6,
    Backlight = 7,
    Unused = 8
  };

  //
  // The following methods are called from main.cpp and are public facing
  //

  // constructor
  ParallaxGen(std::filesystem::path OutputDir, ParallaxGenDirectory *PGD,
              ParallaxGenConfig *PGC, ParallaxGenD3D *PGD3D,
              const bool &OptimizeMeshes = false,
              const bool &IgnoreParallax = false, const bool &IgnoreCM = false,
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
  //
  // The following methods are private and are called from the public facing
  // methods
  //

  // upgrades a height map to complex material
  auto convertHeightMapToComplexMaterial(const std::filesystem::path &HeightMap)
      -> ParallaxGenTask::PGResult;

  // processes a NIF file (enable parallax if needed)
  auto processNIF(const std::filesystem::path &NIFFile,
                  const std::vector<nlohmann::json> &TPBRConfigs)
      -> ParallaxGenTask::PGResult;

  // processes a shape within a NIF file
  auto processShape(const std::filesystem::path &NIFFile,
                    const std::vector<nlohmann::json> &TPBRConfigs,
                    nifly::NifFile &NIF, const uint32_t &ShapeBlockID,
                    nifly::NiShape *NIFShape, bool &NIFModified,
                    const bool &HasAttachedHavok) -> ParallaxGenTask::PGResult;

  // check if truepbr should be enabled on shape
  auto shouldApplyTruePBRConfig(
      const std::filesystem::path &NIFFile,
      const std::vector<nlohmann::json> &TPBRConfigs,
      const uint32_t &ShapeBlockID,
      const std::array<std::string, NUM_TEXTURE_SLOTS> &SearchPrefixes,
      bool &EnableResult,
      std::vector<std::tuple<nlohmann::json, std::string>> &TruePBRData) const
      -> ParallaxGenTask::PGResult;

  // check if complex material should be enabled on shape
  auto shouldEnableComplexMaterial(
      const std::filesystem::path &NIFFile, nifly::NifFile &NIF,
      const uint32_t &ShapeBlockID, nifly::NiShape *NIFShape,
      nifly::NiShader *NIFShader,
      const std::array<std::string, NUM_TEXTURE_SLOTS> &SearchPrefixes,
      bool &EnableResult,
      std::string &MatchedPath) const -> ParallaxGenTask::PGResult;

  // check if vanilla parallax should be enabled on shape
  auto shouldEnableParallax(
      const std::filesystem::path &NIFFile, nifly::NifFile &NIF,
      const uint32_t &ShapeBlockID, nifly::NiShape *NIFShape,
      nifly::NiShader *NIFShader,
      const std::array<std::string, NUM_TEXTURE_SLOTS> &SearchPrefixes,
      const bool &HasAttachedHavok, bool &EnableResult,
      std::string &MatchedPath) const -> ParallaxGenTask::PGResult;

  // applies truepbr config on a shape in a NIF (always applies with config, but
  // maybe PBR is disabled)
  static auto applyTruePBRConfigOnShape(
      nifly::NifFile &NIF, nifly::NiShape *NIFShape, nifly::NiShader *NIFShader,
      nlohmann::json &TruePBRData, const std::string &MatchedPath,
      bool &NIFModified) -> ParallaxGenTask::PGResult;

  // enables truepbr on a shape in a NIF (If PBR is enabled)
  static auto
  enableTruePBROnShape(nifly::NifFile &NIF, nifly::NiShape *NIFShape,
                       nifly::NiShader *NIFShader, nlohmann::json &TruePBRData,
                       const std::string &MatchedPath,
                       bool &NIFModified) -> ParallaxGenTask::PGResult;

  // enables complex material on a shape in a NIF
  static auto enableComplexMaterialOnShape(
      nifly::NifFile &NIF, nifly::NiShape *NIFShape, nifly::NiShader *NIFShader,
      const std::string &MatchedPath, const bool &ApplyDynCubemaps,
      bool &NIFModified) -> ParallaxGenTask::PGResult;

  // enables parallax on a shape in a NIF
  static auto
  enableParallaxOnShape(nifly::NifFile &NIF, nifly::NiShape *NIFShape,
                        nifly::NiShader *NIFShader,
                        const std::string &MatchedPath,
                        bool &NIFModified) -> ParallaxGenTask::PGResult;

  // Gets all the texture prefixes for a textureset. ie. _n.dds is removed etc.
  // for each slot
  static auto getSearchPrefixes(nifly::NifFile &NIF, nifly::NiShape *NIFShape)
      -> std::array<std::string, NUM_TEXTURE_SLOTS>;

  // Zip methods
  void addFileToZip(mz_zip_archive &Zip, const std::filesystem::path &FilePath,
                    const std::filesystem::path &ZipPath) const;

  void zipDirectory(const std::filesystem::path &DirPath,
                    const std::filesystem::path &ZipPath) const;

  // TruePBR Helpers
  // Calculates 2d absolute value of a vector2
  static auto abs2(nifly::Vector2 V) -> nifly::Vector2;

  // Auto UV scale magic that I don't understand
  static auto autoUVScale(const std::vector<nifly::Vector2> *UVS,
                          const std::vector<nifly::Vector3> *Verts,
                          std::vector<nifly::Triangle> &Tris) -> nifly::Vector2;

  // Checks if a json object has a key
  static auto flag(nlohmann::json &JSON, const char *Key) -> bool;

  // Get PBR texture paths given an array of matched prefixes
  static auto getTexturePaths(
      const std::array<std::string, NUM_TEXTURE_SLOTS> &MatchedPaths)
      -> std::array<std::string, NUM_TEXTURE_SLOTS>;
};
