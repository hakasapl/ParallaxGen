#include "ParallaxGen.hpp"

#include <DirectXTex.h>
#include <Geometry.hpp>
#include <NifFile.hpp>
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
#include <ranges>
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
#include "patchers/PatcherShader.hpp"
#include "patchers/PatcherShaderTransform.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D,
                         const bool &OptimizeMeshes)
    : OutputDir(std::move(OutputDir)), PGD(PGD), PGD3D(PGD3D) {
  // constructor

  // set optimize meshes flag
  NIFSaveOptions.optimize = OptimizeMeshes;
}

void ParallaxGen::patchMeshes(const PatcherUtil::PatcherSet &Patchers, const unordered_map<wstring, int> *ModPriority,
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
      boost::asio::post(MeshPatchPool, [this, &TaskTracker, &DiffJSON, &Mesh, &PatchPlugin, &Patchers, &ModPriority] {
        ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
        try {
          Result = processNIF(Patchers, ModPriority, Mesh, &DiffJSON, PatchPlugin);
        } catch (const exception &E) {
          spdlog::error(L"Exception in thread patching NIF {}: {}", Mesh.wstring(), ASCIItoUTF16(E.what()));
          Result = ParallaxGenTask::PGResult::FAILURE;
        }

        TaskTracker.completeJob(Result);
      });
    }

    // verify that all threads complete (should be redundant)
    MeshPatchPool.join();

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(processNIF(Patchers, ModPriority, Mesh, &DiffJSON));
    }
  }

  // Write DiffJSON file
  spdlog::info("Saving diff JSON file...");
  const filesystem::path DiffJSONPath = OutputDir / getDiffJSONName();
  ofstream DiffJSONFile(DiffJSONPath);
  DiffJSONFile << DiffJSON << endl;
  DiffJSONFile.close();
}

auto ParallaxGen::findModConflicts(const PatcherUtil::PatcherSet &Patchers, const bool &MultiThread,
                                   const bool &PatchPlugin)
    -> unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>> {
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
      boost::asio::post(MeshPatchPool, [this, &TaskTracker, &Mesh, &PatchPlugin, &ConflictMods, &ConflictModsMutex,
                                        &Patchers] {
        ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
        try {
          Result = processNIF(Patchers, nullptr, Mesh, nullptr, PatchPlugin, true, &ConflictMods, &ConflictModsMutex);
        } catch (const exception &E) {
          spdlog::error(L"Exception in thread finding mod conflicts {}: {}", Mesh.wstring(), ASCIItoUTF16(E.what()));
          Result = ParallaxGenTask::PGResult::FAILURE;
        }

        TaskTracker.completeJob(Result);
      });
    }

    // verify that all threads complete (should be redundant)
    MeshPatchPool.join();

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(
          processNIF(Patchers, nullptr, Mesh, nullptr, true, true, &ConflictMods, &ConflictModsMutex));
    }
  }

  return ConflictMods;
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
    const PatcherUtil::PatcherSet &Patchers, const unordered_map<wstring, int> *ModPriority,
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
    Logger::error(L"NIF Rejected: Unable to load NIF: {}", ASCIItoUTF16(E.what()));
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Stores whether the NIF has been modified throughout the patching process
  bool NIFModified = false;

  // Create patcher objects
  auto PatcherObjects = PatcherUtil::PatcherObjectSet();
  for (const auto &[Shader, Factory] : Patchers.ShaderPatchers) {
    auto Patcher = Factory(NIFFile, &NIF);
    PatcherObjects.ShaderPatchers.emplace(Shader, std::move(Patcher));
  }
  for (const auto &[Shader, Factory] : Patchers.ShaderTransformPatchers) {
    for (const auto &[TransformShader, TransformFactory] : Factory) {
      auto Transform = TransformFactory(NIFFile, &NIF);
      PatcherObjects.ShaderTransformPatchers[Shader].emplace(TransformShader, std::move(Transform));
    }
  }

  // Patch each shape in NIF
  size_t NumShapes = 0;
  int OldShapeIndex = 0;
  bool OneShapeSuccess = false;
  vector<tuple<NiShape *, int, int>> ShapeTracker;
  for (NiShape *NIFShape : NIF.GetShapes()) {
    // get shape name and blockid
    const auto ShapeBlockID = NIF.GetBlockID(NIFShape);
    const auto ShapeName = ASCIItoUTF16(NIFShape->name.get());
    const auto ShapeIDStr = to_wstring(ShapeBlockID) + L" / " + ShapeName;
    Logger::Prefix PrefixShape(ShapeIDStr);

    NumShapes++;

    bool ShapeModified = false;
    bool ShapeDeleted = false;
    NIFUtil::ShapeShader ShaderApplied = NIFUtil::ShapeShader::NONE;
    ParallaxGenTask::updatePGResult(Result,
                                    processShape(NIFFile, NIF, NIFShape, OldShapeIndex, PatcherObjects, ModPriority,
                                                 ShapeModified, ShapeDeleted, ShaderApplied, Dry, ConflictMods,
                                                 ConflictModsMutex),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    // Update NIFModified if shape was modified
    if (ShapeModified && ShaderApplied != NIFUtil::ShapeShader::NONE) {
      NIFModified = true;

      if (PatchPlugin) {
        // Process plugin
        array<wstring, NUM_TEXTURE_SLOTS> NewSlots;
        ParallaxGenPlugin::processShape(ShaderApplied, NIFFile.wstring(), ShapeName, OldShapeIndex, PatcherObjects,
                                        ModPriority, NewSlots);
      }
    }

    if (Result == ParallaxGenTask::PGResult::SUCCESS) {
      OneShapeSuccess = true;
    }

    if (!ShapeDeleted) {
      ShapeTracker.emplace_back(NIFShape, OldShapeIndex, -1);
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
    // Sort blocks and set plugin indices
    NIF.PrettySortBlocks();

    for (auto &Shape : ShapeTracker) {
      // Find new block id
      const auto NewBlockID = NIF.GetBlockID(get<0>(Shape));
      get<2>(Shape) = static_cast<int>(NewBlockID);
    }

    // Sort ShapeTracker by new block id
    sort(ShapeTracker.begin(), ShapeTracker.end(),
         [](const auto &A, const auto &B) { return get<2>(A) < get<2>(B); });

    // Find new 3d index for each shape
    for (int I = 0; I < ShapeTracker.size(); I++) {
      // get new plugin index
      get<2>(ShapeTracker[I]) = I;
    }

    // Set 3D indices
    ParallaxGenPlugin::set3DIndices(NIFFile.wstring(), ShapeTracker);

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
      auto JSONKey = UTF16toUTF8(NIFFile.wstring());
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
    const PatcherUtil::PatcherObjectSet &Patchers, const unordered_map<wstring, int> *ModPriority, bool &ShapeModified,
    bool &ShapeDeleted, NIFUtil::ShapeShader &ShaderApplied, const bool &Dry,
    unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>> *ConflictMods,
    mutex *ConflictModsMutex) -> ParallaxGenTask::PGResult {
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  Logger::trace(L"Starting Processing");

  // Check for exclusions
  // only allow BSLightingShaderProperty blocks
  string NIFShapeName = NIFShape->GetBlockName();
  if (NIFShapeName != "NiTriShape" && NIFShapeName != "BSTriShape" && NIFShapeName != "BSLODTriShape") {
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
  vector<PatcherUtil::ShaderPatcherMatch> Matches;

  // Restore cache if exists
  {
    lock_guard<mutex> Lock(AllowedShadersCacheMutex);

    // Check if shape has already been processed
    if (AllowedShadersCache.find(CacheKey) != AllowedShadersCache.end()) {
      CacheExists = true;
      Matches = AllowedShadersCache.at(CacheKey);
    }
  }

  // Loop through each shader
  unordered_set<wstring> ModSet;
  if (!CacheExists) {
    for (const auto &[Shader, Patcher] : Patchers.ShaderPatchers) {
        // note: name is defined in source code in UTF8-encoded files
      Logger::Prefix PrefixPatches(UTF8toUTF16(Patcher->getPatcherName()));

      // Check if shader should be applied
      vector<PatcherShader::PatcherMatch> CurMatches;
      if (!Patcher->shouldApply(*NIFShape, CurMatches)) {
        Logger::trace(L"Rejecting: Shader not applicable");
        continue;
      }

      for (const auto &Match : CurMatches) {
        PatcherUtil::ShaderPatcherMatch CurMatch;
        CurMatch.Mod = PGD->getMod(Match.MatchedPath);
        CurMatch.Shader = Shader;
        CurMatch.Match = Match;
        CurMatch.ShaderTransformTo = NIFUtil::ShapeShader::NONE;

        // See if transform is possible
        if (Patchers.ShaderTransformPatchers.contains(Shader)) {
          const auto &AvailableTransforms = Patchers.ShaderTransformPatchers.at(Shader);
          // loop from highest element of map to 0
          for (const auto &AvailableTransform : ranges::reverse_view(AvailableTransforms)) {
            if (Patchers.ShaderPatchers.at(AvailableTransform.first)->canApply(*NIFShape)) {
              // Found a transform that can apply, set the transform in the match
              CurMatch.ShaderTransformTo = AvailableTransform.first;
              break;
            }
          }
        }

        // Add to matches if shader can apply (or if transform shader exists and can apply)
        if (Patcher->canApply(*NIFShape) || CurMatch.ShaderTransformTo != NIFUtil::ShapeShader::NONE) {
          Matches.push_back(CurMatch);
          ModSet.insert(CurMatch.Mod);
        }
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

  // Populate conflict mods if set
  if (ConflictMods != nullptr && ModSet.size() > 1) {
    lock_guard<mutex> Lock(*ConflictModsMutex);

    // add mods to conflict set
    for (const auto &Match : Matches) {
      if (ConflictMods->find(Match.Mod) == ConflictMods->end()) {
        ConflictMods->insert({Match.Mod, {set<NIFUtil::ShapeShader>(), unordered_set<wstring>()}});
      }

      get<0>((*ConflictMods)[Match.Mod]).insert(Match.Shader);
      get<1>((*ConflictMods)[Match.Mod]).insert(ModSet.begin(), ModSet.end());
    }
  }

  if (Dry) {
    // skip if dry run
    return Result;
  }

  // Get winning match
  auto WinningShaderMatch = PatcherUtil::getWinningMatch(Matches, NIFPath, ModPriority);

  // Apply transforms
  WinningShaderMatch = PatcherUtil::applyTransformIfNeeded(WinningShaderMatch, Patchers);

  // loop through patchers
  array<wstring, NUM_TEXTURE_SLOTS> NewSlots =
      Patchers.ShaderPatchers.at(WinningShaderMatch.Shader)
          ->applyPatch(*NIFShape, WinningShaderMatch.Match, ShapeModified, ShapeDeleted);

  if (!ShapeDeleted) {
    ShaderApplied = WinningShaderMatch.Shader;
  }

  // Post warnings if any
  for (const auto &CurMatchedFrom : WinningShaderMatch.Match.MatchedFrom) {
    ParallaxGenWarnings::mismatchWarn(WinningShaderMatch.Match.MatchedPath, NewSlots[static_cast<int>(CurMatchedFrom)]);
  }

  ParallaxGenWarnings::meshWarn(WinningShaderMatch.Match.MatchedPath, NIFPath.wstring());

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

  vector<std::byte> Buffer = getFileBytes(FilePath);

  const filesystem::path RelativePath = FilePath.lexically_relative(OutputDir);
  const string RelativeFilePathAscii = UTF16toASCII(RelativePath.wstring());

  // add file to Zip
  if (mz_zip_writer_add_mem(&Zip, RelativeFilePathAscii.c_str(), Buffer.data(), Buffer.size(), MZ_NO_COMPRESSION) == 0) {
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
  const string ZipPathString = UTF16toUTF8(ZipPath);
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
