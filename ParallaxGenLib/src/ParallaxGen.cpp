#include "ParallaxGen.hpp"

#include <DirectXTex.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/crc.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/thread.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <d3d11.h>

#include "Logger.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"
#include "ParallaxGenWarnings.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherShader.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherVanillaParallax.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC,
                         ParallaxGenD3D *PGD3D, const bool &OptimizeMeshes, const bool &UpgradeShaders)
    : OutputDir(std::move(OutputDir)), PGD(PGD), PGC(PGC), PGD3D(PGD3D), UpgradeShaders(UpgradeShaders) {
  // constructor

  // set optimize meshes flag
  NIFSaveOptions.optimize = OptimizeMeshes;
}

void ParallaxGen::patchMeshes(
    const std::vector<std::function<std::unique_ptr<PatcherShader>(std::filesystem::path, nifly::NifFile *)>> &Patchers,
    const bool &MultiThread, const bool &PatchPlugin) {
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
      boost::asio::post(MeshPatchPool, [this, &TaskTracker, &DiffJSON, &Mesh, &PatchPlugin, &Patchers] {
        ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
        try {
          Result = processNIF(Patchers, Mesh, &DiffJSON, PatchPlugin);
        } catch (const exception &E) {
          spdlog::error(L"Exception in thread patching NIF {}: {}", Mesh.wstring(), strToWstr(E.what()));
          Result = ParallaxGenTask::PGResult::FAILURE;
        }

        TaskTracker.completeJob(Result);
      });
    }

    // verify that all threads complete (should be redundant)
    MeshPatchPool.join();

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(processNIF(Patchers, Mesh, &DiffJSON));
    }
  }

  // Write DiffJSON file
  spdlog::info("Saving diff JSON file...");
  const filesystem::path DiffJSONPath = OutputDir / getDiffJSONName();
  ofstream DiffJSONFile(DiffJSONPath);
  DiffJSONFile << DiffJSON << endl;
  DiffJSONFile.close();
}

auto ParallaxGen::findModConflicts(
    const std::vector<std::function<std::unique_ptr<PatcherShader>(std::filesystem::path, nifly::NifFile *)>> &Patchers,
    const bool &MultiThread,
    const bool &PatchPlugin) -> unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>> {
  auto Meshes = PGD->getMeshes();

  // Create task tracker
  ParallaxGenTask TaskTracker("Finding Mod Conflicts", Meshes.size(), 10);

  // Create thread group
  const boost::thread_group ThreadGroup;

  // Define conflicts
  unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>> ConflictMods;
  mutex ConflictModsMutex;

  // Create threads
  if (MultiThread) {
#ifdef _DEBUG
    size_t NumThreads = 1;
#else
    const size_t NumThreads = boost::thread::hardware_concurrency();
#endif

    boost::asio::thread_pool MeshPatchPool(NumThreads);

    for (const auto &Mesh : Meshes) {
      boost::asio::post(
          MeshPatchPool, [this, &TaskTracker, &Mesh, &PatchPlugin, &ConflictMods, &ConflictModsMutex, &Patchers] {
            ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
            try {
              Result = processNIF(Patchers, Mesh, nullptr, PatchPlugin, true, &ConflictMods, &ConflictModsMutex);
            } catch (const exception &E) {
              spdlog::error(L"Exception in thread finding mod conflicts {}: {}", Mesh.wstring(), strToWstr(E.what()));
              Result = ParallaxGenTask::PGResult::FAILURE;
            }

            TaskTracker.completeJob(Result);
          });
    }

    // verify that all threads complete (should be redundant)
    MeshPatchPool.join();

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(processNIF(Patchers, Mesh, nullptr, true, true, &ConflictMods, &ConflictModsMutex));
    }
  }

  return ConflictMods;
}

auto ParallaxGen::convertHeightMapToComplexMaterial(const filesystem::path &HeightMap,
                                                    wstring &NewCMMap) -> ParallaxGenTask::PGResult {
  lock_guard<mutex> Lock(UpgradeMutex);

  spdlog::trace(L"Upgrading height map: {}", HeightMap.wstring());

  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  const string HeightMapStr = HeightMap.string();

  // Get texture base (remove _p.dds)
  const auto TexBase = NIFUtil::getTexBase(HeightMapStr);
  if (TexBase.empty()) {
    // no height map (this shouldn't happen)
    return Result;
  }

  const filesystem::path ComplexMap = TexBase + L"_m.dds";
  if (PGD->isGenerated(ComplexMap)) {
    // this was already upgraded
    NewCMMap = ComplexMap.wstring();
    return Result;
  }

  static const auto *CMBaseMap = &PGD->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);
  auto ExistingMask = NIFUtil::getTexMatch(TexBase, NIFUtil::TextureType::ENVIRONMENTMASK, *CMBaseMap);
  filesystem::path EnvMask = filesystem::path();
  if (!ExistingMask.empty()) {
    // env mask exists, but it's not a complex material
    // TODO smarter decision here?
    EnvMask = ExistingMask[0].Path;
  }

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
    PGD->getTextureMap(NIFUtil::TextureSlots::ENVMASK)[TexBase].insert(
        {ComplexMap, NIFUtil::TextureType::COMPLEXMATERIAL});

    // Update file map
    auto HeightMapMod = PGD->getMod(HeightMap);
    PGD->addGeneratedFile(ComplexMap, HeightMapMod);
    NewCMMap = ComplexMap.wstring();

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

void ParallaxGen::deleteOutputDir(const bool &PreOutput) const {
  static const unordered_set<filesystem::path> FoldersToDelete = {"meshes", "textures"};
  static const unordered_set<filesystem::path> FilesToDelete = {"ParallaxGen.esp", getDiffJSONName()};
  static const unordered_set<filesystem::path> FilesToIgnore = {"meta.ini"};
  static const unordered_set<filesystem::path> FilesToDeletePreOutput = {getOutputZipName()};

  if (!filesystem::exists(OutputDir) || !filesystem::is_directory(OutputDir)) {
    return;
  }

  for (const auto &Entry : filesystem::directory_iterator(OutputDir)) {
    if (Entry.is_regular_file() &&
        (FilesToDelete.contains(Entry.path().filename()) || FilesToIgnore.contains(Entry.path().filename()) ||
         FilesToDeletePreOutput.contains(Entry.path().filename()))) {
      continue;
    }

    if (Entry.is_directory() && FoldersToDelete.contains(Entry.path().filename())) {
      continue;
    }

    spdlog::critical("Output directory has non-ParallaxGen related files. The output directory should only contain "
                     "files generated by ParallaxGen or empty. Exiting.");
    exit(1);
  }

  spdlog::info("Deleting old output files from output directory...");

  // Delete old output
  for (const auto &FileToDelete : FilesToDelete) {
    const auto File = OutputDir / FileToDelete;
    if (filesystem::exists(File)) {
      filesystem::remove(File);
    }
  }

  for (const auto &FolderToDelete : FoldersToDelete) {
    const auto Folder = OutputDir / FolderToDelete;
    if (filesystem::exists(Folder)) {
      filesystem::remove_all(Folder);
    }
  }

  if (PreOutput) {
    for (const auto &FileToDelete : FilesToDeletePreOutput) {
      const auto File = OutputDir / FileToDelete;
      if (filesystem::exists(File)) {
        filesystem::remove(File);
      }
    }
  }
}

auto ParallaxGen::getOutputZipName() -> filesystem::path { return "ParallaxGen_Output.zip"; }

auto ParallaxGen::getDiffJSONName() -> filesystem::path { return "ParallaxGen_Diff.json"; }

// shorten some enum names
auto ParallaxGen::processNIF(
    const std::vector<std::function<std::unique_ptr<PatcherShader>(std::filesystem::path, nifly::NifFile *)>> &Patchers,
    const filesystem::path &NIFFile, nlohmann::json *DiffJSON, const bool &PatchPlugin, const bool &Dry,
    unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>> *ConflictMods,
    mutex *ConflictModsMutex) -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  Logger::Prefix PrefixNIF(NIFFile.wstring());
  Logger::trace(L"Starting processing");

  // Determine output path for patched NIF
  const filesystem::path OutputFile = OutputDir / NIFFile;
  if (filesystem::exists(OutputFile)) {
    Logger::error(L"NIF Rejected: File already exists");
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Load NIF file
  const vector<std::byte> NIFFileData = PGD->getFile(NIFFile);
  NifFile NIF;
  try {
    NIF = NIFUtil::loadNIFFromBytes(NIFFileData);
  } catch (const exception &E) {
    Logger::error(L"NIF Rejected: Unable to load NIF: {}", strToWstr(E.what()));
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Stores whether the NIF has been modified throughout the patching process
  bool NIFModified = false;

  // Loop through factory
  vector<unique_ptr<PatcherShader>> PatcherList;
  for (const auto &PatcherFactory : Patchers) {
    auto Patcher = PatcherFactory(NIFFile, &NIF);
    PatcherList.push_back(std::move(Patcher));
  }

  // Patch each shape in NIF
  size_t NumShapes = 0;
  int OldShapeIndex = 0;
  int NewShapeIndex = 0;
  bool OneShapeSuccess = false;
  for (NiShape *NIFShape : NIF.GetShapes()) {
    // get shape name and blockid
    const auto ShapeBlockID = NIF.GetBlockID(NIFShape);
    const auto ShapeName = strToWstr(NIFShape->name.get());
    const auto ShapeIDStr = to_wstring(ShapeBlockID) + L" / " + ShapeName;
    Logger::Prefix PrefixShape(ShapeIDStr);

    NumShapes++;

    bool ShapeModified = false;
    bool ShapeDeleted = false;
    NIFUtil::ShapeShader ShaderApplied = NIFUtil::ShapeShader::NONE;
    ParallaxGenTask::updatePGResult(Result,
                                    processShape(NIFFile, NIF, NIFShape, OldShapeIndex, PatcherList, ShapeModified,
                                                 ShapeDeleted, ShaderApplied, Dry, ConflictMods, ConflictModsMutex),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    // Update NIFModified if shape was modified
    if (ShapeModified && ShaderApplied != NIFUtil::ShapeShader::NONE) {
      NIFModified = true;

      if (PatchPlugin) {
        // Process plugin
        array<wstring, NUM_TEXTURE_SLOTS> NewSlots;
        ParallaxGenPlugin::processShape(ShaderApplied, NIFFile.wstring(), ShapeName, OldShapeIndex, NewShapeIndex,
                                        PatcherList, NewSlots);
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
  if (NIFModified && !Dry) {
    // Calculate CRC32 hash before
    boost::crc_32_type CRCBeforeResult{};
    CRCBeforeResult.process_bytes(NIFFileData.data(), NIFFileData.size());
    const auto CRCBefore = CRCBeforeResult.checksum();

    // create directories if required
    filesystem::create_directories(OutputFile.parent_path());

    if (NIF.Save(OutputFile, NIFSaveOptions) != 0) {
      Logger::error(L"Unable to save NIF file");
      Result = ParallaxGenTask::PGResult::FAILURE;
      return Result;
    }

    Logger::debug(L"Saving patched NIF to output");

    // Clear NIF from memory (no longer needed)
    NIF.Clear();

    // Calculate CRC32 hash after
    const auto OutputFileBytes = getFileBytes(OutputFile);
    boost::crc_32_type CRCResultAfter{};
    CRCResultAfter.process_bytes(OutputFileBytes.data(), OutputFileBytes.size());
    const auto CRCAfter = CRCResultAfter.checksum();

    // Add to diff JSON
    if (DiffJSON != nullptr) {
      auto JSONKey = wstrToStr(NIFFile.wstring());
      threadSafeJSONUpdate(
          [&](nlohmann::json &JSON) {
            JSON[JSONKey]["crc32original"] = CRCBefore;
            JSON[JSONKey]["crc32patched"] = CRCAfter;
          },
          *DiffJSON);
    }
  }

  return Result;
}

auto ParallaxGen::processShape(
    const filesystem::path &NIFPath, NifFile &NIF, NiShape *NIFShape, const int &ShapeIndex,
    const vector<unique_ptr<PatcherShader>> &Patchers, bool &ShapeModified, bool &ShapeDeleted,
    NIFUtil::ShapeShader &ShaderApplied, const bool &Dry,
    unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>> *ConflictMods,
    mutex *ConflictModsMutex) -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  Logger::trace(L"Starting Processing");

  // Check for exclusions
  // only allow BSLightingShaderProperty blocks
  string NIFShapeName = NIFShape->GetBlockName();
  if (NIFShapeName != "NiTriShape" && NIFShapeName != "BSTriShape") {
    Logger::trace(L"Rejecting: Incorrect shape block type");
    return Result;
  }

  // get NIFShader type
  if (!NIFShape->HasShaderProperty()) {
    Logger::trace(L"Rejecting: No NIFShader property");
    return Result;
  }

  // get NIFShader from shape
  NiShader *NIFShader = NIF.GetShader(NIFShape);
  if (NIFShader == nullptr) {
    // skip if no NIFShader
    Logger::trace(L"Rejecting: No NIFShader property");
    return Result;
  }

  // check that NIFShader is a BSLightingShaderProperty
  string NIFShaderName = NIFShader->GetBlockName();
  if (NIFShaderName != "BSLightingShaderProperty") {
    Logger::trace(L"Rejecting: Incorrect NIFShader block type");
    return Result;
  }

  // check that NIFShader has a texture set
  if (!NIFShader->HasTextureSet()) {
    Logger::trace(L"Rejecting: No texture set");
    return Result;
  }

  // Create cache key
  bool CacheExists = false;
  ParallaxGen::ShapeKey CacheKey;
  CacheKey.NIFPath = NIFPath;
  CacheKey.ShapeIndex = ShapeIndex;

  // Allowed shaders from result of patchers
  vector<tuple<wstring, NIFUtil::ShapeShader, PatcherShader::PatcherMatch>> Matches;
  {
    lock_guard<mutex> Lock(AllowedShadersCacheMutex);

    // Check if shape has already been processed
    if (AllowedShadersCache.find(CacheKey) != AllowedShadersCache.end()) {
      CacheExists = true;
      Matches = AllowedShadersCache.at(CacheKey);
    }
  }

  // Loop through each shader
  if (!CacheExists) {
    for (const auto &Patcher : Patchers) {
      Logger::Prefix PrefixPatches(strToWstr(Patcher->getPatcherName()));

      // Check if shader should be applied
      vector<PatcherShader::PatcherMatch> CurMatches;
      if (!Patcher->shouldApply(*NIFShape, CurMatches)) {
        Logger::trace(L"Rejecting: Shader not applicable");
        continue;
      }

      for (const auto &Match : CurMatches) {
        Matches.emplace_back(PGD->getMod(Match.MatchedPath), Patcher->getShaderType(), Match);
      }
    }
  }

  // Write to cache if not already present
  if (!CacheExists) {
    lock_guard<mutex> Lock(AllowedShadersCacheMutex);
    AllowedShadersCache[CacheKey] = Matches;
  }

  if (Matches.empty()) {
    // no shaders to apply
    Logger::trace(L"Rejecting: No shaders to apply");
    return Result;
  }

  // create an unordered set of just mods
  unordered_set<wstring> ModSet;
  for (const auto &[Mod, Shader, Match] : Matches) {
    ModSet.insert(Mod);
  }

  // Populate conflict mods if set
  if (ConflictMods != nullptr && ModSet.size() > 1) {
    lock_guard<mutex> Lock(*ConflictModsMutex);

    // add mods to conflict set
    for (const auto &[Mod, Shader, Match] : Matches) {
      if (ConflictMods->find(Mod) == ConflictMods->end()) {
        ConflictMods->insert({Mod, {set<NIFUtil::ShapeShader>(), unordered_set<wstring>()}});
      }

      get<0>((*ConflictMods)[Mod]).insert(Shader);
      get<1>((*ConflictMods)[Mod]).insert(ModSet.begin(), ModSet.end());
    }
  }

  if (Dry) {
    // dry run, no need to apply shaders
    return Result;
  }

  // Find winning mod
  int MaxPriority = -1;
  NIFUtil::ShapeShader WinningShader = NIFUtil::ShapeShader::NONE;
  PatcherShader::PatcherMatch WinningMatch;
  wstring DecisionMod;

  const auto MeshFilePriority = PGC->getModPriority(PGD->getMod(NIFPath));
  for (const auto &[Mod, Shader, Match] : Matches) {
    Logger::Prefix PrefixMod(Mod);
    Logger::trace(L"Checking mod");

    auto CurPriority = PGC->getModPriority(Mod);

    if (CurPriority < MeshFilePriority) {
      // skip mods with lower priority than mesh file
      Logger::trace(L"Rejecting: Mod has lower priority than mesh file");
      continue;
    }

    if (CurPriority < MaxPriority) {
      // skip mods with lower priority than current winner
      Logger::trace(L"Rejecting: Mod has lower priority than current winner");
      continue;
    }

    Logger::trace(L"Mod accepted");
    MaxPriority = CurPriority;
    WinningShader = Shader;
    WinningMatch = Match;
    DecisionMod = Mod;
  }

  Logger::trace(L"Winning mod: {}", DecisionMod);

  // Upgrade shader if required
  if (WinningShader == NIFUtil::ShapeShader::VANILLAPARALLAX && UpgradeShaders) {
    // convert then change shader type
    wstring NewCMMap;
    convertHeightMapToComplexMaterial(WinningMatch.MatchedPath, NewCMMap);
    WinningMatch.MatchedPath = NewCMMap;
    WinningShader = NIFUtil::ShapeShader::COMPLEXMATERIAL;
  }

  // loop through patchers
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots;
  for (const auto &Patcher : Patchers) {
    if (Patcher->getShaderType() != WinningShader) {
      continue;
    }

    // apply patch
    NewSlots = Patcher->applyPatch(*NIFShape, WinningMatch, ShapeModified, ShapeDeleted);

    if (!ShapeDeleted) {
      ShaderApplied = WinningShader;
    }

    break;
  }

  // Post warnings if any
  for (const auto &CurMatchedFrom : WinningMatch.MatchedFrom) {
    ParallaxGenWarnings::mismatchWarn(WinningMatch.MatchedPath, NewSlots[static_cast<int>(CurMatchedFrom)]);
  }

  ParallaxGenWarnings::meshWarn(WinningMatch.MatchedPath, NIFPath.wstring());

  return Result;
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
