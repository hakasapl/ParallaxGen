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

#include <d3d11.h>

#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUI.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(filesystem::path OutputDir, ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC,
                         ParallaxGenD3D *PGD3D, ModManagerDirectory *MMD, const bool &OptimizeMeshes,
                         const bool &IgnoreParallax, const bool &IgnoreCM, const bool &IgnoreTruePBR,
                         const bool &UpgradeShaders, const bool &MMEnabled)
    : OutputDir(std::move(OutputDir)), MMD(MMD), PGD(PGD), PGC(PGC), PGD3D(PGD3D), IgnoreParallax(IgnoreParallax),
      IgnoreCM(IgnoreCM), IgnoreTruePBR(IgnoreTruePBR), UpgradeShaders(UpgradeShaders), MMEnabled(MMEnabled) {
  // constructor

  // set optimize meshes flag
  NIFSaveOptions.optimize = OptimizeMeshes;
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

    // UI loop
    while (!TaskTracker.isCompleted()) {
      ParallaxGenUI::updateUI();
    }

    // verify that all threads complete (should be redundant)
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
auto ParallaxGen::processNIF(const filesystem::path &NIFFile, nlohmann::json &DiffJSON,
                             const bool &PatchPlugin) -> ParallaxGenTask::PGResult {
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
  } catch (const exception &E) {
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
    ParallaxGenTask::updatePGResult(
        Result,
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
                               PatcherTruePBR &PatchTPBR, bool &ShapeModified, bool &ShapeDeleted,
                               NIFUtil::ShapeShader &ShaderApplied) -> ParallaxGenTask::PGResult {
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

  unordered_map<wstring, pair<NIFUtil::ShapeShader, wstring>> AllowedShaders;
  pair<NIFUtil::ShapeShader, wstring> LastAllowedShader;

  // Custom fields that are not used for every shader
  unordered_map<wstring, map<size_t, tuple<nlohmann::json, wstring>>> AllowedTruePBRData;

  // VANILLA PARALLAX
  if (!IgnoreParallax) {
    bool ShouldApply = false;
    vector<wstring> MatchedPaths;
    ParallaxGenTask::updatePGResult(Result,
                                    PatchVP.shouldApply(NIFShape, SearchPrefixes, OldSlots, ShouldApply, MatchedPaths),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    for (const auto &MatchedPath : MatchedPaths) {
      // get mod for matched path (in this case a texture)
      auto Mod = PGD->getMod(MatchedPath);
      // TODO if user does not specify mod manager then we need to use the one that is already in the mesh where
      // possible (put in the last spot of the vector)
      AllowedShaders[Mod] = {NIFUtil::ShapeShader::VANILLAPARALLAX, MatchedPath};
      LastAllowedShader = {NIFUtil::ShapeShader::VANILLAPARALLAX, MatchedPath};
    }
  }

  // COMPLEX MATERIAL
  if (!IgnoreCM) {
    bool ShouldApply = false;
    vector<wstring> MatchedPaths;
    ParallaxGenTask::updatePGResult(Result,
                                    PatchCM.shouldApply(NIFShape, SearchPrefixes, OldSlots, ShouldApply, MatchedPaths),
                                    ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    for (const auto &MatchedPath : MatchedPaths) {
      // get mod for matched path (in this case a texture)
      auto Mod = PGD->getMod(MatchedPath);
      // TODO if user does not specify mod manager then we need to use the one that is already in the mesh where
      // possible (put in the last spot of the vector)
      AllowedShaders[Mod] = {NIFUtil::ShapeShader::COMPLEXMATERIAL, MatchedPath};
      LastAllowedShader = {NIFUtil::ShapeShader::COMPLEXMATERIAL, MatchedPath};
    }
  }

  // TRUEPBR CONFIG
  if (!IgnoreTruePBR) {
    bool ShouldApply = false;
    vector<wstring> MatchedPaths;
    vector<map<size_t, tuple<nlohmann::json, wstring>>> TruePBRData;
    ParallaxGenTask::updatePGResult(
        Result, PatchTPBR.shouldApply(NIFShape, SearchPrefixes, ShouldApply, TruePBRData, MatchedPaths),
        ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);

    for (size_t I = 0; I < TruePBRData.size(); I++) {
      auto Mod = PGD->getMod(MatchedPaths[I]);
      AllowedTruePBRData[Mod] = TruePBRData[I];
      AllowedShaders[Mod] = {NIFUtil::ShapeShader::TRUEPBR, MatchedPaths[I]};
      LastAllowedShader = {NIFUtil::ShapeShader::TRUEPBR, MatchedPaths[I]};
    }
  }

  if (LastAllowedShader.first == NIFUtil::ShapeShader::NONE) {
    // no shaders to apply
    spdlog::trace(L"NIF: {} | Shape: {} | Rejecting: No shaders to apply", NIFPath.wstring(), ShapeBlockID);
    return Result;
  }

  NIFUtil::ShapeShader WinningShader = LastAllowedShader.first;
  wstring WinningMatchedPath;
  wstring DecisionMod;
  if (MMEnabled) {
    // Create modset vector
    vector<wstring> AllowedMods;
    AllowedMods.reserve(AllowedShaders.size());
    for (const auto &[Mod, Shader] : AllowedShaders) {
      AllowedMods.push_back(Mod);
    }

    // Find winner
    if (AllowedShaders.size() == 1) {
      DecisionMod = AllowedShaders.begin()->first;
    } else {
      // lock so we don't get prompted a bunch of times
      lock_guard<mutex> Lock(PromptMutex);

      // check if we have a rule for this modset
      DecisionMod = PGC->getModsetRule(AllowedMods);
    }

    if (DecisionMod.empty()) {
      // we must ask the user to decide
      DecisionMod = promptForSelection(AllowedShaders);

      // Set user decision for future use
      PGC->setModsetRule(AllowedMods, DecisionMod);
    }

    WinningShader = AllowedShaders[DecisionMod].first;
    WinningMatchedPath = AllowedShaders[DecisionMod].second;
  } else {
    // just use the last shader
    WinningShader = LastAllowedShader.first;
    WinningMatchedPath = LastAllowedShader.second;
  }

  // Upgrade shader if required
  if (WinningShader == NIFUtil::ShapeShader::VANILLAPARALLAX && UpgradeShaders) {
    // convert then change shader type
    convertHeightMapToComplexMaterial(WinningMatchedPath, WinningMatchedPath);
    WinningShader = NIFUtil::ShapeShader::COMPLEXMATERIAL;
  }

  if (WinningShader == NIFUtil::ShapeShader::TRUEPBR) {
    auto WinningTruePBRData = AllowedTruePBRData[DecisionMod];

    for (auto &TruePBRCFG : WinningTruePBRData) {
      spdlog::trace(L"NIF: {} | Shape: {} | PBR | Applying PBR Config {}", NIFPath.wstring(), ShapeBlockID,
                    TruePBRCFG.first);
      ParallaxGenTask::updatePGResult(Result,
                                      PatchTPBR.applyPatch(NIFShape, get<0>(TruePBRCFG.second),
                                                           get<1>(TruePBRCFG.second), ShapeModified, ShapeDeleted));
    }

    if (!ShapeDeleted) {
      ShaderApplied = NIFUtil::ShapeShader::TRUEPBR;
    }
  } else if (WinningShader == NIFUtil::ShapeShader::COMPLEXMATERIAL) {
    ParallaxGenTask::updatePGResult(Result, PatchCM.applyPatch(NIFShape, WinningMatchedPath, ShapeModified));
    ShaderApplied = NIFUtil::ShapeShader::COMPLEXMATERIAL;
  } else if (WinningShader == NIFUtil::ShapeShader::VANILLAPARALLAX) {
    ParallaxGenTask::updatePGResult(Result, PatchVP.applyPatch(NIFShape, WinningMatchedPath, ShapeModified));
    ShaderApplied = NIFUtil::ShapeShader::VANILLAPARALLAX;
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
      spdlog::warn(L"Some NIFs were patched from textures from different mods (in many cases this is intentional and "
                   L"can be ignored): {}, {}",
                   BaseMod, MatchedMod);
      MismatchTracker.insert(MatchPair);
    }
  }
}

auto ParallaxGen::promptForSelection(const unordered_map<wstring, pair<NIFUtil::ShapeShader, wstring>> &Mods)
    -> wstring {
  lock_guard<mutex> Lock(PromptMutex);

  vector<wstring> ModList;
  vector<wstring> ModListPretty;
  for (const auto &[Mod, Shader] : Mods) {
    // add to options
    wstring ShaderStr;
    switch (Shader.first) {
    case NIFUtil::ShapeShader::VANILLAPARALLAX:
      if (UpgradeShaders) {
        ShaderStr = L"Complex Material (Upgraded)";
      } else {
        ShaderStr = L"Parallax";
      }
      break;
    case NIFUtil::ShapeShader::COMPLEXMATERIAL:
      ShaderStr = L"Complex Material";
      break;
    case NIFUtil::ShapeShader::TRUEPBR:
      ShaderStr = L"PBR";
      break;
    default:
      ShaderStr = L"Unknown";
      break;
    }

    auto SelectStr = Mod + L" (" + ShaderStr + L")";
    ModListPretty.push_back(SelectStr);
    ModList.push_back(Mod);
  }

  // prompt user
  auto Selection = ParallaxGenUI::promptForSelection("Multiple mods conflict. Select which mod should have priority.",
                                                     ModListPretty);

  // return selected mod
  return ModList[Selection];
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
