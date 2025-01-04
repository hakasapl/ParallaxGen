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
#include "patchers/PatcherUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenD3D *PGD3D,
                         const bool &OptimizeMeshes)
    : OutputDir(std::move(OutputDir)), PGD(PGD), PGD3D(PGD3D), ModPriority(nullptr) {
  // constructor

  // set optimize meshes flag
  NIFSaveOptions.optimize = OptimizeMeshes;
}

void ParallaxGen::loadPatchers(const PatcherUtil::PatcherSet &Patchers) { this->Patchers = Patchers; }

void ParallaxGen::loadModPriorityMap(unordered_map<wstring, int> *ModPriority) {
  this->ModPriority = ModPriority;
  ParallaxGenPlugin::loadModPriorityMap(ModPriority);
}

void ParallaxGen::patchMeshes(const bool &MultiThread, const bool &PatchPlugin) {
  auto Meshes = PGD->getMeshes();

  // Create task tracker
  ParallaxGenTask TaskTracker("Mesh Patcher", Meshes.size());

  // Create thread group
  const boost::thread_group ThreadGroup;

  // Define diff JSON
  nlohmann::json DiffJSON = nlohmann::json::object();

  // Create threads
  if (MultiThread) {
#ifdef _DEBUG
    size_t NumThreads = 1;
#else
    const size_t NumThreads = boost::thread::hardware_concurrency();
#endif

    boost::asio::thread_pool MeshPatchPool(NumThreads);

    atomic<bool> KillThreads = false;

    for (const auto &Mesh : Meshes) {
      boost::asio::post(MeshPatchPool, [this, &TaskTracker, &DiffJSON, &Mesh, &PatchPlugin, &KillThreads] {
        ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
        try {
          Result = processNIF(Mesh, &DiffJSON, PatchPlugin);
        } catch (const exception &E) {
          spdlog::error(L"Exception in thread patching NIF {}: {}", Mesh.wstring(), ASCIItoUTF16(E.what()));
          KillThreads.store(true, std::memory_order_release);
          Result = ParallaxGenTask::PGResult::FAILURE;
        }

        TaskTracker.completeJob(Result);
      });
    }

    while (!TaskTracker.isCompleted()) {
      if (KillThreads.load(std::memory_order_acquire)) {
        MeshPatchPool.stop();
        throw runtime_error("Exception in thread patching NIF");
      }

      this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // verify that all threads complete (should be redundant)
    MeshPatchPool.join();

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(processNIF(Mesh, &DiffJSON));
    }
  }

  ParallaxGenWarnings::printWarnings();

  // Write DiffJSON file
  spdlog::info("Saving diff JSON file...");
  const filesystem::path DiffJSONPath = OutputDir / getDiffJSONName();
  ofstream DiffJSONFile(DiffJSONPath);
  DiffJSONFile << DiffJSON << endl;
  DiffJSONFile.close();
}

auto ParallaxGen::findModConflicts(const bool &MultiThread, const bool &PatchPlugin)
    -> unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>> {
  auto Meshes = PGD->getMeshes();

  // Create task tracker
  ParallaxGenTask TaskTracker("Finding Mod Conflicts", Meshes.size(), 10);

  // Create thread group
  const boost::thread_group ThreadGroup;

  // Define conflicts
  PatcherUtil::ConflictModResults ConflictMods;

  // Create threads
  if (MultiThread) {
#ifdef _DEBUG
    size_t NumThreads = 1;
#else
    const size_t NumThreads = boost::thread::hardware_concurrency();
#endif

    atomic<bool> KillThreads = false;

    boost::asio::thread_pool MeshPatchPool(NumThreads);

    for (const auto &Mesh : Meshes) {
      boost::asio::post(MeshPatchPool, [this, &TaskTracker, &Mesh, &PatchPlugin, &KillThreads, &ConflictMods] {
        ParallaxGenTask::PGResult Result = ParallaxGenTask::PGResult::SUCCESS;
        try {
          Result = processNIF(Mesh, nullptr, PatchPlugin, &ConflictMods);
        } catch (const exception &E) {
          spdlog::error(L"Exception in thread finding mod conflicts {}: {}", Mesh.wstring(), ASCIItoUTF16(E.what()));
          KillThreads.store(true, std::memory_order_release);
          Result = ParallaxGenTask::PGResult::FAILURE;
        }

        TaskTracker.completeJob(Result);
      });
    }

    while (!TaskTracker.isCompleted()) {
      if (KillThreads.load(std::memory_order_acquire)) {
        MeshPatchPool.stop();
        throw runtime_error("Exception in thread finding mod conflicts");
      }

      this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // verify that all threads complete (should be redundant)
    MeshPatchPool.join();

  } else {
    for (const auto &Mesh : Meshes) {
      TaskTracker.completeJob(processNIF(Mesh, nullptr, true, &ConflictMods));
    }
  }

  return ConflictMods.Mods;
}

void ParallaxGen::zipMeshes() const {
  // Zip meshes
  spdlog::info("Zipping meshes...");
  zipDirectory(OutputDir, OutputDir / getOutputZipName());
}

void ParallaxGen::deleteOutputDir(const bool &PreOutput) const {
  static const unordered_set<filesystem::path> FoldersToDelete = {"meshes", "textures", "LightPlacer"};
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

auto ParallaxGen::processNIF(const filesystem::path &NIFFile, nlohmann::json *DiffJSON, const bool &PatchPlugin,
                             PatcherUtil::ConflictModResults *ConflictMods) -> ParallaxGenTask::PGResult {
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
  vector<std::byte> NIFFileData;
  try {
    NIFFileData = PGD->getFile(NIFFile);
  } catch (const exception &E) {
    Logger::error(L"NIF Rejected: Unable to load NIF: {}", ASCIItoUTF16(E.what()));
    Result = ParallaxGenTask::PGResult::FAILURE;
    return Result;
  }

  // Process NIF
  bool NIFModified = false;
  vector<pair<filesystem::path, nifly::NifFile>> DupNIFs;
  auto NIF = processNIF(NIFFile, NIFFileData, NIFModified, nullptr, &DupNIFs, PatchPlugin, ConflictMods);

  // Save patched NIF if it was modified
  if (NIFModified && ConflictMods == nullptr) {
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

  // Save any duplicate NIFs
  for (auto &[DupNIFFile, DupNIF] : DupNIFs) {
    const auto DupNIFPath = OutputDir / DupNIFFile;
    // TODO do we need to add info about this to diff json?
    if (DupNIF.Save(DupNIFPath, NIFSaveOptions) != 0) {
      Logger::error(L"Unable to save duplicate NIF file");
      Result = ParallaxGenTask::PGResult::FAILURE;
      return Result;
    }
  }

  return Result;
}

// shorten some enum names
auto ParallaxGen::processNIF(const std::filesystem::path &NIFFile, const vector<std::byte> &NIFBytes, bool &NIFModified,
                             const vector<NIFUtil::ShapeShader> *ForceShaders,
                             vector<pair<filesystem::path, nifly::NifFile>> *DupNIFs, const bool &PatchPlugin,
                             PatcherUtil::ConflictModResults *ConflictMods) -> nifly::NifFile {
  NifFile NIF;
  try {
    NIF = NIFUtil::loadNIFFromBytes(NIFBytes);
  } catch (const exception &E) {
    Logger::error(L"NIF Rejected: Unable to load NIF: {}", ASCIItoUTF16(E.what()));
    return {};
  }

  NIFModified = false;

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
  for (const auto &Factory : Patchers.GlobalPatchers) {
    auto Patcher = Factory(NIFFile, &NIF);
    PatcherObjects.GlobalPatchers.emplace_back(std::move(Patcher));
  }

  // Get shapes
  auto Shapes = NIF.GetShapes();

  // Store plugin records
  vector<NIFUtil::ShapeShader> ShadersAppliedMesh(Shapes.size(), NIFUtil::ShapeShader::UNKNOWN);
  unordered_map<int, pair<vector<NIFUtil::ShapeShader>, vector<ParallaxGenPlugin::TXSTResult>>> RecordHandleTracker;

  // Patch each shape in NIF
  size_t NumShapes = 0;
  int OldShapeIndex = 0;
  vector<tuple<NiShape *, int, int>> ShapeTracker;
  for (NiShape *NIFShape : Shapes) {
    // Check for any non-ascii chars
    for (uint32_t Slot = 0; Slot < NUM_TEXTURE_SLOTS; Slot++) {
      string Texture;
      NIF.GetTextureSlot(NIFShape, Texture, Slot);

      if (!ContainsOnlyAscii(Texture)) {
        spdlog::error(L"NIF {} has texture slot(s) with invalid non-ASCII chars (skipping)", NIFFile.wstring());
        return {};
      }
    }

    // get shape name and blockid
    const auto ShapeBlockID = NIF.GetBlockID(NIFShape);
    const auto ShapeName = ASCIItoUTF16(NIFShape->name.get());
    const auto ShapeIDStr = to_wstring(ShapeBlockID) + L" / " + ShapeName;
    Logger::Prefix PrefixShape(ShapeIDStr);

    NumShapes++;

    bool ShapeModified = false;
    bool ShapeDeleted = false;
    NIFUtil::ShapeShader ShaderApplied = NIFUtil::ShapeShader::NONE;

    ParallaxGenTask::PGResult ShapeResult = ParallaxGenTask::PGResult::SUCCESS;
    if (ForceShaders != nullptr && OldShapeIndex < ForceShaders->size()) {
      // Force shaders
      auto ShaderForce = (*ForceShaders)[OldShapeIndex];
      ShapeResult = processShape(NIFFile, NIF, NIFShape, OldShapeIndex, PatcherObjects, ShapeModified, ShapeDeleted,
                                 ShaderApplied, ConflictMods, &ShaderForce);
    } else {
      ShapeResult = processShape(NIFFile, NIF, NIFShape, OldShapeIndex, PatcherObjects, ShapeModified, ShapeDeleted,
                                 ShaderApplied, ConflictMods);
    }

    if (ShapeResult != ParallaxGenTask::PGResult::SUCCESS) {
      // Fail on process shape fail
      return {};
    }

    NIFModified |= ShapeModified;
    ShadersAppliedMesh[OldShapeIndex] = ShaderApplied;

    // Update NIFModified if shape was modified
    if (PatchPlugin) {
      // Get all plugin results
      vector<ParallaxGenPlugin::TXSTResult> Results;
      ParallaxGenPlugin::processShape(NIFFile.wstring(), NIFShape, ShapeName, OldShapeIndex, PatcherObjects, Results,
                                      ConflictMods);

      // Loop through results
      for (const auto &Result : Results) {
        if (RecordHandleTracker.find(Result.ModelRecHandle) == RecordHandleTracker.end()) {
          RecordHandleTracker[Result.ModelRecHandle] = {
              vector<NIFUtil::ShapeShader>(Shapes.size(), NIFUtil::ShapeShader::UNKNOWN), {}};
        }

        RecordHandleTracker[Result.ModelRecHandle].first[OldShapeIndex] = Result.Shader;
        RecordHandleTracker[Result.ModelRecHandle].second.push_back(Result);
      }
    }

    if (!ShapeDeleted) {
      ShapeTracker.emplace_back(NIFShape, OldShapeIndex, -1);
    }

    OldShapeIndex++;
  }

  if (ConflictMods != nullptr) {
    // no need to continue if just getting mod conflicts
    return NIF;
  }

  if (PatchPlugin) {
    // Loop through plugin results and fix unknowns to match mesh
    for (auto &[ModelRecHandle, Results] : RecordHandleTracker) {
      for (size_t I = 0; I < Shapes.size(); I++) {
        if (Results.first[I] == NIFUtil::ShapeShader::UNKNOWN) {
          Results.first[I] = ShadersAppliedMesh[I];
        }
      }
    }

    unordered_map<vector<NIFUtil::ShapeShader>, wstring, VectorHash> MeshTracker;

    // Apply plugin results
    size_t NumMesh = 0;
    for (const auto &[ModelRecHandle, Results] : RecordHandleTracker) {
      wstring NewNIFName;
      if (MeshTracker.contains(Results.first)) {
        // Mesh already exists, we only need to assign mesh
        NewNIFName = MeshTracker[Results.first];
      } else {
        const auto CurShaders = Results.first;
        if (CurShaders == ShadersAppliedMesh) {
          NewNIFName = NIFFile.wstring();
        } else {
          // Different from mesh which means duplicate is needed
          NewNIFName = (NIFFile.parent_path() / NIFFile.stem()).wstring() + L"_pg" + to_wstring(++NumMesh) + L".nif";
          // Create duplicate NIF object from original bytes
          bool DupNIFModified = false;
          auto DupNIF = processNIF(NewNIFName, NIFBytes, DupNIFModified, &CurShaders);
          if (DupNIFs != nullptr) {
            DupNIFs->emplace_back(NewNIFName, DupNIF);
          }
        }
      }

      // assign mesh in plugin
      ParallaxGenPlugin::assignMesh(NewNIFName, Results.second);

      // Add to mesh tracker
      MeshTracker[Results.first] = NewNIFName;
    }
  }

  // Run global patchers
  for (const auto &GlobalPatcher : PatcherObjects.GlobalPatchers) {
    Logger::Prefix PrefixPatches(UTF8toUTF16(GlobalPatcher->getPatcherName()));
    GlobalPatcher->applyPatch(NIFModified);
  }

  // Delete unreferenced blocks
  NIF.DeleteUnreferencedBlocks();

  // Sort blocks and set plugin indices
  NIF.PrettySortBlocks();

  if (PatchPlugin) {
    for (auto &Shape : ShapeTracker) {
      // Find new block id
      const auto NewBlockID = NIF.GetBlockID(get<0>(Shape));
      get<2>(Shape) = static_cast<int>(NewBlockID);
    }

    // Sort ShapeTracker by new block id
    sort(ShapeTracker.begin(), ShapeTracker.end(), [](const auto &A, const auto &B) { return get<2>(A) < get<2>(B); });

    // Find new 3d index for each shape
    for (int I = 0; I < ShapeTracker.size(); I++) {
      // get new plugin index
      get<2>(ShapeTracker[I]) = I;
    }

    ParallaxGenPlugin::set3DIndices(NIFFile.wstring(), ShapeTracker);
  }

  return NIF;
}

auto ParallaxGen::processShape(const filesystem::path &NIFPath, NifFile &NIF, NiShape *NIFShape, const int &ShapeIndex,
                               PatcherUtil::PatcherObjectSet &Patchers, bool &ShapeModified, bool &ShapeDeleted,
                               NIFUtil::ShapeShader &ShaderApplied, PatcherUtil::ConflictModResults *ConflictMods,
                               const NIFUtil::ShapeShader *ForceShader) -> ParallaxGenTask::PGResult {

  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  Logger::trace(L"Starting Processing");

  // Check for exclusions
  // only allow BSLightingShaderProperty blocks
  string NIFShapeName = NIFShape->GetBlockName();
  if (NIFShapeName != "NiTriShape" && NIFShapeName != "BSTriShape" && NIFShapeName != "BSLODTriShape" &&
      NIFShapeName != "BSMeshLODTriShape") {
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

  if (ForceShader != nullptr) {
    // Force shader means we can just apply the shader and return
    ShaderApplied = *ForceShader;
    // Find correct patcher
    const auto &Patcher = Patchers.ShaderPatchers.at(*ForceShader);
    Patcher->applyShader(*NIFShape, ShapeModified);

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

    {
      // write to cache
      lock_guard<mutex> Lock(AllowedShadersCacheMutex);
      AllowedShadersCache[CacheKey] = Matches;
    }
  }

  if (Matches.empty()) {
    // no shaders to apply
    Logger::trace(L"Rejecting: No shaders to apply");
    return Result;
  }

  // Populate conflict mods if set
  if (ConflictMods != nullptr) {
    if (ModSet.size() > 1) {
      lock_guard<mutex> Lock(ConflictMods->Mutex);

      // add mods to conflict set
      for (const auto &Match : Matches) {
        if (ConflictMods->Mods.find(Match.Mod) == ConflictMods->Mods.end()) {
          ConflictMods->Mods.insert({Match.Mod, {set<NIFUtil::ShapeShader>(), unordered_set<wstring>()}});
        }

        get<0>(ConflictMods->Mods[Match.Mod]).insert(Match.Shader);
        get<1>(ConflictMods->Mods[Match.Mod]).insert(ModSet.begin(), ModSet.end());
      }
    }

    return Result;
  }

  // Get winning match
  auto WinningShaderMatch = PatcherUtil::getWinningMatch(Matches, ModPriority);

  // Apply transforms
  WinningShaderMatch = PatcherUtil::applyTransformIfNeeded(WinningShaderMatch, Patchers);

  // Fix num texture slots
  // TODO move to patcher at some point?
  auto *TXSTRec = NIF.GetHeader().GetBlock(NIFShader->TextureSetRef());
  if (TXSTRec->textures.size() < 9) {
    TXSTRec->textures.resize(9);
    ShapeModified = true;
  }

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
  if (mz_zip_writer_add_mem(&Zip, RelativeFilePathAscii.c_str(), Buffer.data(), Buffer.size(), MZ_NO_COMPRESSION) ==
      0) {
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
