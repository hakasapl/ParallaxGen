#include "ParallaxGen.hpp"

#include <DirectXTex.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/crc.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <vector>

#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC,
                         ParallaxGenD3D *PGD3D, const bool &OptimizeMeshes, const bool &IgnoreParallax,
                         const bool &IgnoreCM, const bool &IgnoreTruePBR)
    : OutputDir(std::move(OutputDir)), PGD(PGD), PGC(PGC), PGD3D(PGD3D), IgnoreParallax(IgnoreParallax),
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

  for (const auto &HeightMap : HeightMaps) {
    TaskTracker.completeJob(convertHeightMapToComplexMaterial(HeightMap.second.Path));
  }
}

void ParallaxGen::patchMeshes(const bool &MultiThread) {
  auto Meshes = PGD->getMeshes();

  // Create task tracker
  ParallaxGenTask TaskTracker("Mesh Patcher", Meshes.size());

  // Create thread group
  boost::thread_group ThreadGroup;

  // Define diff JSON
  nlohmann::json DiffJSON;

  // Create threads
  if (MultiThread) {
#ifdef _DEBUG
    size_t NumThreads = 1;
#else
    size_t NumThreads = boost::thread::hardware_concurrency();
#endif

    boost::asio::thread_pool MeshPatchPool(NumThreads);

    // Exception vars
    exception_ptr ThreadException = nullptr;
    mutex ExceptionMutex;

    for (const auto &Mesh : Meshes) {
      boost::asio::post(MeshPatchPool, [this, &TaskTracker, &DiffJSON, Mesh, &ThreadException, &ExceptionMutex] {
        try {
          TaskTracker.completeJob(processNIF(Mesh, DiffJSON));
        } catch (...) {
          lock_guard<mutex> Lock(ExceptionMutex);
          ThreadException = current_exception();
        }
      });
    }

    // Wait for all threads to complete
    MeshPatchPool.join();

    // Rethrow exception if one occurred
    if (ThreadException) {
      rethrow_exception(ThreadException);
    }

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(processNIF(Mesh, DiffJSON));
    }
  }

  // Write DiffJSON file
  spdlog::info("Saving diff JSON file...");
  filesystem::path DiffJSONPath = OutputDir / getDiffJSONName();
  ofstream DiffJSONFile(DiffJSONPath);
  DiffJSONFile << DiffJSON << endl;
  DiffJSONFile.close();
}

auto ParallaxGen::convertHeightMapToComplexMaterial(const filesystem::path &HeightMap) -> ParallaxGenTask::PGResult {
  spdlog::trace(L"Upgrading height map: {}", HeightMap.wstring());

  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  string HeightMapStr = HeightMap.string();

  // Get texture base (remove _p.dds)
  const auto TexBase = NIFUtil::getTexBase(HeightMapStr);
  if (TexBase.empty()) {
    // no height map (this shouldn't happen)
    return Result;
  }

  static const auto *CMBaseMap = &PGD->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);
  auto ExistingCM = NIFUtil::getTexMatch(TexBase, *CMBaseMap);
  filesystem::path EnvMask = filesystem::path();
  if (!ExistingCM.Path.empty()) {
    if (ExistingCM.Type == NIFUtil::TextureType::COMPLEXMATERIAL) {
      // complex material already exists
      return Result;
    }

    // env mask exists, but it's not a complex material
    EnvMask = ExistingCM.Path;
  }

  const filesystem::path ComplexMap = TexBase + L"_m.dds";

  // upgrade to complex material
  DirectX::ScratchImage NewComplexMap = PGD3D->upgradeToComplexMaterial(HeightMap, EnvMask);

  // save to file
  if (NewComplexMap.GetImageCount() > 0) {
    filesystem::path OutputPath = OutputDir / ComplexMap;
    filesystem::create_directories(OutputPath.parent_path());

    HRESULT HR = DirectX::SaveToDDSFile(NewComplexMap.GetImages(), NewComplexMap.GetImageCount(),
                                        NewComplexMap.GetMetadata(), DirectX::DDS_FLAGS_NONE, OutputPath.c_str());
    if (FAILED(HR)) {
      spdlog::error(L"Unable to save complex material {}: {}", OutputPath.wstring(),
                    strToWstr(ParallaxGenD3D::getHRESULTErrorMessage(HR)));
      Result = ParallaxGenTask::PGResult::FAILURE;
      return Result;
    }

    // add newly created file to complexMaterialMaps for later processing
    PGD->getTextureMap(NIFUtil::TextureSlots::ENVMASK)[TexBase] = {ComplexMap, NIFUtil::TextureType::COMPLEXMATERIAL};

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
auto ParallaxGen::processNIF(const filesystem::path &NIFFile, nlohmann::json &DiffJSON) -> ParallaxGenTask::PGResult {
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
  PatcherVanillaParallax PatchVP(NIFFile, &NIF, PGC, PGD, PGD3D);
  PatcherComplexMaterial PatchCM(NIFFile, &NIF, PGC, PGD, PGD3D);
  PatcherTruePBR PatchTPBR(NIFFile, &NIF, PGD);

  // Patch each shape in NIF
  size_t NumShapes = 0;
  bool OneShapeSuccess = false;
  for (NiShape *NIFShape : NIF.GetShapes()) {
    NumShapes++;

    bool ShapeModified = false;
    string ShaderApplied;
    ParallaxGenTask::updatePGResult(Result,
                                    processShape(NIFFile, NIF, NIFShape, PatchVP, PatchCM, PatchTPBR, ShapeModified),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    // Update NIFModified if shape was modified
    if (ShapeModified) {
      NIFModified = true;
    }

    if (Result == ParallaxGenTask::PGResult::SUCCESS) {
      OneShapeSuccess = true;
    }
  }

  if (!OneShapeSuccess && NumShapes > 0) {
    // No shapes were successfully processed
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Save patched NIF if it was modified
  if (NIFModified) {
    // Calculate CRC32 hash before
    boost::crc_32_type CRCBeforeResult;
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
    boost::crc_32_type CRCResultAfter;
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
                               PatcherTruePBR &PatchTPBR, bool &ShapeModified) const -> ParallaxGenTask::PGResult {
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
  auto SearchPrefixes = NIFUtil::getSearchPrefixes(NIF, NIFShape);
  wstring MatchedPath;

  // TRUEPBR CONFIG
  if (!IgnoreTruePBR) {
    bool EnableTruePBR = false;
    map<size_t, tuple<nlohmann::json, wstring>> TruePBRData;
    ParallaxGenTask::updatePGResult(Result,
                                    PatchTPBR.shouldApply(NIFShape, SearchPrefixes, EnableTruePBR, TruePBRData),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
    if (EnableTruePBR) {
      // Enable TruePBR on shape
      for (auto &TruePBRCFG : TruePBRData) {
        spdlog::trace(L"NIF: {} | Shape: {} | PBR | Applying PBR Config {}", NIFPath.wstring(), ShapeBlockID,
                      TruePBRCFG.first);
        ParallaxGenTask::updatePGResult(Result, PatchTPBR.applyPatch(NIFShape, get<0>(TruePBRCFG.second),
                                                                     get<1>(TruePBRCFG.second), ShapeModified));
      }

      return Result;
    }
  }

  // COMPLEX MATERIAL
  if (!IgnoreCM) {
    bool EnableCM = false;
    bool EnableDynCubemaps = false;
    MatchedPath = L"";
    ParallaxGenTask::updatePGResult(
        Result, PatchCM.shouldApply(NIFShape, SearchPrefixes, EnableCM, EnableDynCubemaps, MatchedPath),
        ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
    if (EnableCM) {
      // Enable complex material on shape
      ParallaxGenTask::updatePGResult(Result,
                                      PatchCM.applyPatch(NIFShape, MatchedPath, EnableDynCubemaps, ShapeModified));

      return Result;
    }
  }

  // VANILLA PARALLAX
  if (!IgnoreParallax) {
    bool EnableParallax = false;
    MatchedPath = L"";
    ParallaxGenTask::updatePGResult(Result, PatchVP.shouldApply(NIFShape, SearchPrefixes, EnableParallax, MatchedPath),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
    if (EnableParallax) {
      // Enable Parallax on shape
      ParallaxGenTask::updatePGResult(Result, PatchVP.applyPatch(NIFShape, MatchedPath, ShapeModified));

      return Result;
    }
  }

  return Result;
}

void ParallaxGen::threadSafeJSONUpdate(const std::function<void(nlohmann::json &)> &Operation,
                                       nlohmann::json &DiffJSON) {
  std::lock_guard<std::mutex> Lock(JSONUpdateMutex);
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
  filesystem::path ZipRelativePath = FilePath.lexically_relative(OutputDir);

  string ZipFilePath = wstrToStr(ZipRelativePath.wstring());

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
  string ZipPathString = wstrToStr(ZipPath);
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
