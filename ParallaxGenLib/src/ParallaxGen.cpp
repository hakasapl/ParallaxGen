#include "ParallaxGen.hpp"

#include <DirectXTex.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/crc.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC,
                         ParallaxGenD3D *PGD3D, ModManagerDirectory *MMD, const bool &OptimizeMeshes, const bool &IgnoreParallax,
                         const bool &IgnoreCM, const bool &IgnoreTruePBR)
    : OutputDir(std::move(OutputDir)), MMD(MMD), PGD(PGD), PGC(PGC), PGD3D(PGD3D), IgnoreParallax(IgnoreParallax),
      IgnoreCM(IgnoreCM), IgnoreTruePBR(IgnoreTruePBR) {
  // constructor

  // set optimize meshes flag
  NIFSaveOptions.optimize = OptimizeMeshes;
}

void ParallaxGen::upgradeShaders() {
  // Get height maps (vanilla _p.dds files)
  const auto &HeightMaps = PGD->getTextureMapConst(NIFUtil::TextureSlots::PARALLAX);

  // Define task parameters
  ParallaxGenTask TaskTracker("Shader Upgrades", HeightMaps.size());

  for (const auto &HeightSlot : HeightMaps) {
    for (const auto &HeightMap : HeightSlot.second) {
      if (HeightMap.Type != NIFUtil::TextureType::HEIGHT) {
        // not a height map
        continue;
      }

      TaskTracker.completeJob(convertHeightMapToComplexMaterial(HeightMap.Path));
    }
  }
}

void ParallaxGen::patchMeshes(const bool &MultiThread, const bool &PatchPlugin) {
  auto Meshes = PGD->getMeshes();

  // Create task tracker
  ParallaxGenTask TaskTracker("Mesh Patcher", Meshes.size());

  // Create thread group
  const boost::thread_group ThreadGroup;

  // Define diff JSON
  nlohmann::json DiffJSON;

  // Create threads
  if (MultiThread) {
#ifdef _DEBUG
    size_t NumThreads = 1;
#else
    const size_t NumThreads = boost::thread::hardware_concurrency();
#endif

    boost::asio::thread_pool MeshPatchPool(NumThreads);

    for (const auto &Mesh : Meshes) {
      boost::asio::post(MeshPatchPool, [this, &TaskTracker, &DiffJSON, &Mesh, &PatchPlugin] {
        ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
        try {
          Result = processNIF(Mesh, DiffJSON, PatchPlugin);
        } catch (const exception &E) {
          spdlog::error(L"Exception in thread patching NIF {}: {}", Mesh.wstring(), strToWstr(E.what()));
          Result = ParallaxGenTask::PGResult::FAILURE;
        }

        TaskTracker.completeJob(Result);
      });
    }

    // Wait for all threads to complete
    MeshPatchPool.join();

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(processNIF(Mesh, DiffJSON));
    }
  }

  // Write DiffJSON file
  spdlog::info("Saving diff JSON file...");
  const filesystem::path DiffJSONPath = OutputDir / getDiffJSONName();
  ofstream DiffJSONFile(DiffJSONPath);
  DiffJSONFile << DiffJSON << endl;
  DiffJSONFile.close();
}

auto ParallaxGen::convertHeightMapToComplexMaterial(const filesystem::path &HeightMap) -> ParallaxGenTask::PGResult {
  spdlog::trace(L"Upgrading height map: {}", HeightMap.wstring());

  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  const string HeightMapStr = HeightMap.string();

  // Get texture base (remove _p.dds)
  const auto TexBase = NIFUtil::getTexBase(HeightMapStr);
  if (TexBase.empty()) {
    // no height map (this shouldn't happen)
    return Result;
  }

  static const auto *CMBaseMap = &PGD->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);
  // TODO this needs to account for priority
  auto ExistingCM = NIFUtil::getTexMatch(TexBase, NIFUtil::TextureType::COMPLEXMATERIAL, *CMBaseMap);
  auto HeightMapMod = PGD->getMod(HeightMap);
  for (const auto &Existing : ExistingCM) {
    // check priority of this file
    if (MMD->getModPriority(PGD->getMod(Existing.Path)) >= MMD->getModPriority(HeightMapMod)) {
      // existing complex material has higher or same priority
      return Result;
    }
  }

  auto ExistingMask = NIFUtil::getTexMatch(TexBase, NIFUtil::TextureType::ENVIRONMENTMASK, *CMBaseMap);
  filesystem::path EnvMask = filesystem::path();
  if (!ExistingMask.empty()) {
    // env mask exists, but it's not a complex material
    // TODO smarter decision here?
    EnvMask = ExistingMask[0].Path;
  }

  const filesystem::path ComplexMap = TexBase + L"_m.dds";

  // upgrade to complex material
  const DirectX::ScratchImage NewComplexMap = PGD3D->upgradeToComplexMaterial(HeightMap, EnvMask);

  // save to file
  if (NewComplexMap.GetImageCount() > 0) {
    const filesystem::path OutputPath = OutputDir / ComplexMap;
    filesystem::create_directories(OutputPath.parent_path());

    const HRESULT HR = DirectX::SaveToDDSFile(NewComplexMap.GetImages(), NewComplexMap.GetImageCount(),
                                        NewComplexMap.GetMetadata(), DirectX::DDS_FLAGS_NONE, OutputPath.c_str());
    if (FAILED(HR)) {
      spdlog::error(L"Unable to save complex material {}: {}", OutputPath.wstring(),
                    strToWstr(ParallaxGenD3D::getHRESULTErrorMessage(HR)));
      Result = ParallaxGenTask::PGResult::FAILURE;
      return Result;
    }

    // add newly created file to complexMaterialMaps for later processing
    PGD->getTextureMap(NIFUtil::TextureSlots::ENVMASK)[TexBase].insert({ComplexMap, NIFUtil::TextureType::COMPLEXMATERIAL});

    // Update file map
    PGD->addGeneratedFile(ComplexMap, HeightMapMod);

    spdlog::debug(L"Generated complex material map: {}", ComplexMap.wstring());
  } else {
    Result = ParallaxGenTask::PGResult::FAILURE;
  }

  return Result;
}

void ParallaxGen::zipMeshes() const {
  // Zip meshes
  spdlog::info("Zipping meshes...");
  zipDirectory(OutputDir, OutputDir / getOutputZipName());
}

void ParallaxGen::deleteMeshes() const {
  // delete meshes
  spdlog::info("Cleaning up meshes generated by ParallaxGen...");
  // Iterate through the folder
  for (const auto &Entry : filesystem::directory_iterator(OutputDir)) {
    if (filesystem::is_directory(Entry.path())) {
      // Remove the directory and all its contents
      try {
        filesystem::remove_all(Entry.path());
        spdlog::trace(L"Deleted directory {}", Entry.path().wstring());
      } catch (const exception &E) {
        spdlog::error(L"Error deleting directory {}: {}", Entry.path().wstring(), strToWstr(E.what()));
      }
    }

    // remove stray files except output
    if (!boost::equals(Entry.path().filename().wstring(), getOutputZipName().wstring())) {
      try {
        filesystem::remove(Entry.path());
      } catch (const exception &E) {
        spdlog::error(L"Error deleting state file {}: {}", Entry.path().wstring(), strToWstr(E.what()));
      }
    }
  }
}

void ParallaxGen::deleteOutputDir() const {
  // delete output directory
  if (filesystem::exists(OutputDir) && filesystem::is_directory(OutputDir)) {
    spdlog::info("Deleting existing ParallaxGen output...");

    try {
      for (const auto &Entry : filesystem::directory_iterator(OutputDir)) {
        filesystem::remove_all(Entry.path());
      }
    } catch (const exception &E) {
      spdlog::critical(L"Error deleting output directory {}: {}", OutputDir.wstring(), strToWstr(E.what()));
      exit(1);
    }
  }
}

auto ParallaxGen::getOutputZipName() -> filesystem::path { return "ParallaxGen_Output.zip"; }

auto ParallaxGen::getDiffJSONName() -> filesystem::path { return "ParallaxGen_Diff.json"; }

// shorten some enum names
auto ParallaxGen::processNIF(const filesystem::path &NIFFile, nlohmann::json &DiffJSON, const bool &PatchPlugin) -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  spdlog::trace(L"NIF: {} | Starting processing", NIFFile.wstring());

  // Determine output path for patched NIF
  const filesystem::path OutputFile = OutputDir / NIFFile;
  if (filesystem::exists(OutputFile)) {
    spdlog::error(L"NIF: {} | NIF Rejected: File already exists", NIFFile.wstring());
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Load NIF file
  const vector<std::byte> NIFFileData = PGD->getFile(NIFFile);
  NifFile NIF;
  try {
    NIF = NIFUtil::loadNIFFromBytes(NIFFileData);
  } catch(const exception &E) {
    spdlog::error(L"NIF: {} | NIF Rejected: Unable to load NIF: {}", NIFFile.wstring(), strToWstr(E.what()));
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Stores whether the NIF has been modified throughout the patching process
  bool NIFModified = false;

  // Create Patcher objects
  // TODO make more of these static
  PatcherVanillaParallax PatchVP(NIFFile, &NIF);
  PatcherComplexMaterial PatchCM(NIFFile, &NIF);
  PatcherTruePBR PatchTPBR(NIFFile, &NIF);

  // Patch each shape in NIF
  size_t NumShapes = 0;
  int OldShapeIndex = 0;
  int NewShapeIndex = 0;
  bool OneShapeSuccess = false;
  for (NiShape *NIFShape : NIF.GetShapes()) {
    NumShapes++;

    bool ShapeModified = false;
    bool ShapeDeleted = false;
    NIFUtil::ShapeShader ShaderApplied = NIFUtil::ShapeShader::NONE;
    ParallaxGenTask::updatePGResult(Result,
                                    processShape(NIFFile, NIF, NIFShape, PatchVP, PatchCM, PatchTPBR, ShapeModified, ShapeDeleted, ShaderApplied),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    // Update NIFModified if shape was modified
    if (ShapeModified) {
      NIFModified = true;

      if (PatchPlugin) {
        // Process plugin
        auto ShapeName = strToWstr(NIFShape->name.get());
        ParallaxGenPlugin::processShape(ShaderApplied, NIFFile.wstring(), ShapeName, OldShapeIndex, NewShapeIndex);
      }
    }

    if (Result == ParallaxGenTask::PGResult::SUCCESS) {
      OneShapeSuccess = true;
    }

    if (!ShapeDeleted) {
      NewShapeIndex++;
    }

    OldShapeIndex++;
  }

  if (!OneShapeSuccess && NumShapes > 0) {
    // No shapes were successfully processed
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Save patched NIF if it was modified
  if (NIFModified) {
    // Calculate CRC32 hash before
    boost::crc_32_type CRCBeforeResult{};
    CRCBeforeResult.process_bytes(NIFFileData.data(), NIFFileData.size());
    const auto CRCBefore = CRCBeforeResult.checksum();

    // create directories if required
    filesystem::create_directories(OutputFile.parent_path());

    if (NIF.Save(OutputFile, NIFSaveOptions) != 0) {
      spdlog::error(L"Unable to save NIF file: {}", NIFFile.wstring());
      Result = ParallaxGenTask::PGResult::FAILURE;
      return Result;
    }

    spdlog::debug(L"NIF: {} | Saving patched NIF to output", NIFFile.wstring());

    // Clear NIF from memory (no longer needed)
    NIF.Clear();

    // Calculate CRC32 hash after
    const auto OutputFileBytes = getFileBytes(OutputFile);
    boost::crc_32_type CRCResultAfter{};
    CRCResultAfter.process_bytes(OutputFileBytes.data(), OutputFileBytes.size());
    const auto CRCAfter = CRCResultAfter.checksum();

    // Add to diff JSON
    auto JSONKey = wstrToStr(NIFFile.wstring());
    threadSafeJSONUpdate(
        [&](nlohmann::json &JSON) {
          JSON[JSONKey]["crc32original"] = CRCBefore;
          JSON[JSONKey]["crc32patched"] = CRCAfter;
        },
        DiffJSON);
  }

  return Result;
}

auto ParallaxGen::processShape(const filesystem::path &NIFPath, NifFile &NIF, NiShape *NIFShape,
                               PatcherVanillaParallax &PatchVP, PatcherComplexMaterial &PatchCM,
                               PatcherTruePBR &PatchTPBR, bool &ShapeModified, bool &ShapeDeleted, NIFUtil::ShapeShader &ShaderApplied) -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  const auto ShapeBlockID = NIF.GetBlockID(NIFShape);
  spdlog::trace(L"NIF: {} | Shape: {} | Starting Processing", NIFPath.wstring(), ShapeBlockID);

  // Check for exclusions
  // only allow BSLightingShaderProperty blocks
  string NIFShapeName = NIFShape->GetBlockName();
  if (NIFShapeName != "NiTriShape" && NIFShapeName != "BSTriShape") {
    spdlog::trace(L"NIF: {} | Shape: {} | Rejecting: Incorrect shape block type", NIFPath.wstring(), ShapeBlockID);
    return Result;
  }

  // get NIFShader type
  if (!NIFShape->HasShaderProperty()) {
    spdlog::trace(L"NIF: {} | Shape: {} | Rejecting: No NIFShader property", NIFPath.wstring(), ShapeBlockID);
    return Result;
  }

  // get NIFShader from shape
  NiShader *NIFShader = NIF.GetShader(NIFShape);
  if (NIFShader == nullptr) {
    // skip if no NIFShader
    spdlog::trace(L"NIF: {} | Shape: {} | Rejecting: No NIFShader property", NIFPath.wstring(), ShapeBlockID);
    return Result;
  }

  // check that NIFShader is a BSLightingShaderProperty
  string NIFShaderName = NIFShader->GetBlockName();
  if (NIFShaderName != "BSLightingShaderProperty") {
    spdlog::trace(L"NIF: {} | Shape: {} | Rejecting: Incorrect NIFShader block type", NIFPath.wstring(), ShapeBlockID);
    return Result;
  }

  // check that NIFShader has a texture set
  if (!NIFShader->HasTextureSet()) {
    spdlog::trace(L"NIF: {} | Shape: {} | Rejecting: No texture set", NIFPath.wstring(), ShapeBlockID);
    return Result;
  }

  // Find search prefixes
  auto OldSlots = NIFUtil::getTextureSlots(NIF, NIFShape);
  auto SearchPrefixes = NIFUtil::getSearchPrefixes(NIF, NIFShape);
  auto WinningShader = NIFUtil::ShapeShader::NONE;
  wstring WinningMod;
  int WinningModPriority = -1;
  wstring WinningMatchedPath;

  // Custom fields that are not always used
  bool EnableDynCubemaps = false;
  map<size_t, tuple<nlohmann::json, wstring>> TruePBRData;

  // TODO don't duplciate this code so much
  // VANILLA PARALLAX
  if (!IgnoreParallax) {
    bool ShouldApply = false;
    wstring MatchedPath;
    ParallaxGenTask::updatePGResult(Result, PatchVP.shouldApply(NIFShape, SearchPrefixes, OldSlots, ShouldApply, MatchedPath),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    if (ShouldApply) {
      // get mod for matched path (in this case a texture)
      auto Mod = PGD->getMod(MatchedPath);
      auto ModPriority = MMD->getModPriority(Mod);

      if (ModPriority >= WinningModPriority) {
        WinningMod = Mod;
        WinningModPriority = ModPriority;
        WinningShader = NIFUtil::ShapeShader::VANILLAPARALLAX;
        WinningMatchedPath = MatchedPath;
      }
    }
  }

  // COMPLEX MATERIAL
  if (!IgnoreCM) {
    bool ShouldApply = false;
    wstring MatchedPath;
    ParallaxGenTask::updatePGResult(
        Result, PatchCM.shouldApply(NIFShape, SearchPrefixes, OldSlots, ShouldApply, EnableDynCubemaps, MatchedPath),
        ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    if (ShouldApply) {
      // get mod for matched path (in this case a texture)
      auto Mod = PGD->getMod(MatchedPath);
      auto ModPriority = MMD->getModPriority(Mod);

      if (ModPriority >= WinningModPriority) {
        WinningMod = Mod;
        WinningModPriority = ModPriority;
        WinningShader = NIFUtil::ShapeShader::COMPLEXMATERIAL;
        WinningMatchedPath = MatchedPath;
      }
    }
  }

  // TRUEPBR CONFIG
  if (!IgnoreTruePBR) {
    bool ShouldApply = false;
    wstring MatchedPath;
    ParallaxGenTask::updatePGResult(Result,
                                    PatchTPBR.shouldApply(NIFShape, SearchPrefixes, ShouldApply, TruePBRData, MatchedPath),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
    if (ShouldApply) {
      // get mod for matched path (in this case a JSON)
      auto Mod = PGD->getMod(MatchedPath);
      auto ModPriority = MMD->getModPriority(Mod);

      if (ModPriority >= WinningModPriority) {
        WinningMod = Mod;
        WinningModPriority = ModPriority;
        WinningShader = NIFUtil::ShapeShader::TRUEPBR;
        WinningMatchedPath = MatchedPath;
      }
    }
  }

  // Determine if diffuse comes from a different mod
  findModMismatch(OldSlots[static_cast<int>(NIFUtil::TextureSlots::DIFFUSE)], WinningMod);
  findModMismatch(OldSlots[static_cast<int>(NIFUtil::TextureSlots::NORMAL)], WinningMod);

  // apply winning patch
  switch (WinningShader) {
  case NIFUtil::ShapeShader::TRUEPBR:
    // Enable TruePBR on shape
    for (auto &TruePBRCFG : TruePBRData) {
      spdlog::trace(L"NIF: {} | Shape: {} | PBR | Applying PBR Config {}", NIFPath.wstring(), ShapeBlockID,
                    TruePBRCFG.first);
      ParallaxGenTask::updatePGResult(Result, PatchTPBR.applyPatch(NIFShape, get<0>(TruePBRCFG.second),
                                                                    get<1>(TruePBRCFG.second), ShapeModified, ShapeDeleted));
    }

    if (!ShapeDeleted) {
      ShaderApplied = NIFUtil::ShapeShader::TRUEPBR;
    }

    break;
  case NIFUtil::ShapeShader::COMPLEXMATERIAL:
    // Enable complex material on shape
    ParallaxGenTask::updatePGResult(Result,
                                      PatchCM.applyPatch(NIFShape, WinningMatchedPath, EnableDynCubemaps, ShapeModified));
    ShaderApplied = NIFUtil::ShapeShader::COMPLEXMATERIAL;

    break;
  case NIFUtil::ShapeShader::VANILLAPARALLAX:
    // Enable Parallax on shape
    ParallaxGenTask::updatePGResult(Result, PatchVP.applyPatch(NIFShape, WinningMatchedPath, ShapeModified));
    ShaderApplied = NIFUtil::ShapeShader::VANILLAPARALLAX;

    break;
  default:
    break;
  }

  return Result;
}

void ParallaxGen::findModMismatch(const wstring &BaseFile, const wstring &MatchedMod) {
  auto BaseMod = PGD->getMod(BaseFile);
  if (!BaseMod.empty() && !MatchedMod.empty() && !boost::iequals(MatchedMod, BaseMod)) {
    // Mismatch detected
    auto MatchPair = make_pair(BaseMod, MatchedMod);
    // check if in map
    if (MismatchTracker.find(MatchPair) == MismatchTracker.end()) {
      // not in map, add to map
      spdlog::warn(L"Some NIFs were patched from textures from different mods (in many cases this is intentional and can be ignored): {}, {}", BaseMod, MatchedMod);
      MismatchTracker.insert(MatchPair);
    }
  }
}

void ParallaxGen::threadSafeJSONUpdate(const std::function<void(nlohmann::json &)> &Operation,
                                       nlohmann::json &DiffJSON) {
  const std::lock_guard<std::mutex> Lock(JSONUpdateMutex);
  Operation(DiffJSON);
}

void ParallaxGen::addFileToZip(mz_zip_archive &Zip, const filesystem::path &FilePath,
                               const filesystem::path &ZipPath) const {
  // ignore Zip file itself
  if (FilePath == ZipPath) {
    return;
  }

  // open file stream
  vector<std::byte> Buffer = getFileBytes(FilePath);

  // get relative path
  const filesystem::path ZipRelativePath = FilePath.lexically_relative(OutputDir);

  const string ZipFilePath = wstrToStr(ZipRelativePath.wstring());

  // add file to Zip
  if (mz_zip_writer_add_mem(&Zip, ZipFilePath.c_str(), Buffer.data(), Buffer.size(), MZ_NO_COMPRESSION) == 0) {
    spdlog::error(L"Error adding file to zip: {}", FilePath.wstring());
    exit(1);
  }
}

void ParallaxGen::zipDirectory(const filesystem::path &DirPath, const filesystem::path &ZipPath) const {
  mz_zip_archive Zip;

  // init to 0
  memset(&Zip, 0, sizeof(Zip));

  // Check if file already exists and delete
  if (filesystem::exists(ZipPath)) {
    spdlog::info(L"Deleting existing output Zip file: {}", ZipPath.wstring());
    filesystem::remove(ZipPath);
  }

  // initialize file
  const string ZipPathString = wstrToStr(ZipPath);
  if (mz_zip_writer_init_file(&Zip, ZipPathString.c_str(), 0) == 0) {
    spdlog::critical(L"Error creating Zip file: {}", ZipPath.wstring());
    exit(1);
  }

  // add each file in directory to Zip
  for (const auto &Entry : filesystem::recursive_directory_iterator(DirPath)) {
    if (filesystem::is_regular_file(Entry.path())) {
      addFileToZip(Zip, Entry.path(), ZipPath);
    }
  }

  // finalize Zip
  if (mz_zip_writer_finalize_archive(&Zip) == 0) {
    spdlog::critical(L"Error finalizing Zip archive: {}", ZipPath.wstring());
    exit(1);
  }

  mz_zip_writer_end(&Zip);

  spdlog::info(L"Please import this file into your mod manager: {}", ZipPath.wstring());
}
