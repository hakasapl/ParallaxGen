#include "ParallaxGenPlugin.hpp"

#include <mutex>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <winbase.h>

#include "Logger.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenMutagenWrapperNE.h"
#include "ParallaxGenUtil.hpp"
#include "ParallaxGenWarnings.hpp"
#include "patchers/PatcherUtil.hpp"

#include <format>

using namespace std;

namespace {
void dnne_failure(enum failure_type type, int error_code) {
  if (type == failure_type::failure_load_runtime) {
    if (error_code == 0x80008096) { // FrameworkMissingFailure from
                                    // https://github.com/dotnet/runtime/blob/main/src/native/corehost/error_codes.h
      spdlog::critical("The required .NET runtime is missing");
    } else {
      spdlog::critical("Could not load .NET runtime, error code {:#X}", error_code);
    }
  } else if (type == failure_type::failure_load_export) {
    spdlog::critical("Could not load .NET exports, error code {:#X}", error_code);
  }
  exit(1);
}
} // namespace

mutex ParallaxGenPlugin::LibMutex;

void ParallaxGenPlugin::libLogMessageIfExists() {
  int Level = 0;
  wchar_t *Message = nullptr;
  GetLogMessage(&Message, &Level);

  while (Message != nullptr) {
    wstring MessageOut(Message);
    LocalFree(static_cast<HGLOBAL>(Message)); // Only free if memory was allocated.
    Message = nullptr;

    // log the message
    switch (Level) {
    case 0:
      spdlog::trace(MessageOut);
      break;
    case 1:
      spdlog::debug(MessageOut);
      break;
    case 2:
      spdlog::info(MessageOut);
      break;
    case 3:
      spdlog::warn(MessageOut);
      break;
    case 4:
      spdlog::error(MessageOut);
      break;
    case 5:
      spdlog::critical(MessageOut);
      break;
    }

    // Get the next message
    GetLogMessage(&Message, &Level);
  }
}

void ParallaxGenPlugin::libThrowExceptionIfExists() {
  wchar_t *Message = nullptr;
  GetLastException(&Message);

  if (Message == nullptr) {
    return;
  }

  wstring MessageOut(Message);
  LocalFree(static_cast<HGLOBAL>(Message)); // Only free if memory was allocated.

  throw runtime_error("ParallaxGenMutagenWrapper.dll: " + ParallaxGenUtil::wstrToStr(MessageOut));
}

void ParallaxGenPlugin::libInitialize(const int &GameType, const wstring &DataPath, const wstring &OutputPlugin,
                                      const vector<wstring> &LoadOrder) {
  lock_guard<mutex> Lock(LibMutex);

  // Use std::vector to manage the memory for LoadOrderArr
  vector<const wchar_t *> LoadOrderArr;
  if (!LoadOrder.empty()) {
    LoadOrderArr.reserve(LoadOrder.size()); // Pre-allocate the vector size
    for (const auto &Mod : LoadOrder) {
      LoadOrderArr.push_back(Mod.c_str()); // Populate the vector with the c_str pointers
    }
  }

  // Add the null terminator to the end
  LoadOrderArr.push_back(nullptr);

  Initialize(GameType, DataPath.c_str(), OutputPlugin.c_str(), LoadOrderArr.data());
  libLogMessageIfExists();
  libThrowExceptionIfExists();
}

void ParallaxGenPlugin::libPopulateObjs() {
  lock_guard<mutex> Lock(LibMutex);

  PopulateObjs();
  libLogMessageIfExists();
  libThrowExceptionIfExists();
}

void ParallaxGenPlugin::libFinalize(const filesystem::path &OutputPath) {
  lock_guard<mutex> Lock(LibMutex);

  Finalize(OutputPath.c_str());
  libLogMessageIfExists();
  libThrowExceptionIfExists();
}

auto ParallaxGenPlugin::libGetMatchingTXSTObjs(const wstring &NIFName, const wstring &Name3D,
                                               const int &Index3D) -> vector<tuple<int, int>> {
  lock_guard<mutex> Lock(LibMutex);

  int Length = 0;
  GetMatchingTXSTObjs(NIFName.c_str(), Name3D.c_str(), Index3D, nullptr, nullptr, &Length);
  libLogMessageIfExists();
  libThrowExceptionIfExists();

  vector<int> TXSTIdArray(Length);
  vector<int> AltTexIdArray(Length);
  GetMatchingTXSTObjs(NIFName.c_str(), Name3D.c_str(), Index3D, TXSTIdArray.data(), AltTexIdArray.data(), nullptr);
  libLogMessageIfExists();
  libThrowExceptionIfExists();

  vector<tuple<int, int>> OutputArray(Length);
  for (int I = 0; I < Length; ++I) {
    OutputArray[I] = {TXSTIdArray[I], AltTexIdArray[I]};
  }

  return OutputArray;
}

auto ParallaxGenPlugin::libGetTXSTSlots(const int &TXSTIndex) -> array<wstring, NUM_TEXTURE_SLOTS> {
  lock_guard<mutex> Lock(LibMutex);

  wchar_t *SlotsArray[NUM_TEXTURE_SLOTS] = {nullptr};
  auto OutputArray = array<wstring, NUM_TEXTURE_SLOTS>();

  // Call the function
  GetTXSTSlots(TXSTIndex, SlotsArray);
  libLogMessageIfExists();
  libThrowExceptionIfExists();

  // Process the slots
  for (int I = 0; I < NUM_TEXTURE_SLOTS; ++I) {
    if (SlotsArray[I] != nullptr) {
      // Convert to C-style string (Ansi)
      const auto *SlotStr = static_cast<const wchar_t *>(SlotsArray[I]);
      OutputArray[I] = SlotStr;

      // Free the unmanaged memory allocated by Marshal.StringToHGlobalAnsi
      LocalFree(static_cast<HGLOBAL>(SlotsArray[I]));
    }
  }

  return OutputArray;
}

void ParallaxGenPlugin::libCreateTXSTPatch(const int &TXSTIndex, const array<wstring, NUM_TEXTURE_SLOTS> &Slots) {
  lock_guard<mutex> Lock(LibMutex);

  // Prepare the array of const wchar_t* pointers from the Slots array
  const wchar_t *SlotsArray[NUM_TEXTURE_SLOTS] = {nullptr};
  for (int I = 0; I < NUM_TEXTURE_SLOTS; ++I) {
    SlotsArray[I] = Slots[I].c_str(); // Point to the internal wide string data of each wstring
  }

  // Call the CreateTXSTPatch function with TXSTIndex and the array of wide string pointers
  CreateTXSTPatch(TXSTIndex, SlotsArray);
  libLogMessageIfExists();
  libThrowExceptionIfExists();
}

auto ParallaxGenPlugin::libCreateNewTXSTPatch(const int &AltTexIndex,
                                              const array<wstring, NUM_TEXTURE_SLOTS> &Slots) -> int {
  lock_guard<mutex> Lock(LibMutex);

  // Prepare the array of const wchar_t* pointers from the Slots array
  const wchar_t *SlotsArray[NUM_TEXTURE_SLOTS] = {nullptr};
  for (int I = 0; I < NUM_TEXTURE_SLOTS; ++I) {
    SlotsArray[I] = Slots[I].c_str(); // Point to the internal wide string data of each wstring
  }

  // Call the CreateNewTXSTPatch function with AltTexIndex and the array of wide string pointers
  int NewTXSTId = 0;
  CreateNewTXSTPatch(AltTexIndex, SlotsArray, &NewTXSTId);
  libLogMessageIfExists();
  libThrowExceptionIfExists();

  return NewTXSTId;
}

void ParallaxGenPlugin::libSetModelAltTex(const int &AltTexIndex, const int &TXSTIndex) {
  lock_guard<mutex> Lock(LibMutex);

  SetModelAltTex(AltTexIndex, TXSTIndex);
  libLogMessageIfExists();
  libThrowExceptionIfExists();
}

void ParallaxGenPlugin::libSet3DIndex(const int &AltTexIndex, const int &Index3D) {
  lock_guard<mutex> Lock(LibMutex);

  Set3DIndex(AltTexIndex, Index3D);
  libLogMessageIfExists();
  libThrowExceptionIfExists();
}

auto ParallaxGenPlugin::libGetTXSTFormID(const int &TXSTIndex) -> tuple<unsigned int, wstring> {
  lock_guard<mutex> Lock(LibMutex);

  wchar_t *PluginName = nullptr;
  unsigned int FormID = 0;
  GetTXSTFormID(TXSTIndex, &FormID, &PluginName);
  libLogMessageIfExists();
  libThrowExceptionIfExists();

  wstring PluginNameString;
  if (PluginName != nullptr) {
    PluginNameString = wstring(PluginName);
    LocalFree(static_cast<HGLOBAL>(PluginName)); // Only free if memory was allocated.
  } else {
    // Handle the case where PluginName is null (e.g., log an error, throw an exception, etc.).
    PluginNameString = L"Unknown";
  }

  return make_tuple(FormID, PluginNameString);
}

mutex ParallaxGenPlugin::TXSTModMapMutex;
unordered_map<int, unordered_map<NIFUtil::ShapeShader, int>> ParallaxGenPlugin::TXSTModMap; // NOLINT

mutex ParallaxGenPlugin::TXSTWarningMapMutex;
unordered_map<int, NIFUtil::ShapeShader> ParallaxGenPlugin::TXSTWarningMap; // NOLINT

ParallaxGenDirectory *ParallaxGenPlugin::PGD;

mutex ParallaxGenPlugin::ProcessShapeMutex;

void ParallaxGenPlugin::loadStatics(ParallaxGenDirectory *PGD) { ParallaxGenPlugin::PGD = PGD; }

void ParallaxGenPlugin::initialize(const BethesdaGame &Game) {
  set_failure_callback(dnne_failure);

  // Maps BethesdaGame::GameType to Mutagen game type
  static const unordered_map<BethesdaGame::GameType, int> MutagenGameTypeMap = {
      {BethesdaGame::GameType::SKYRIM, 1},     {BethesdaGame::GameType::SKYRIM_SE, 2},
      {BethesdaGame::GameType::SKYRIM_VR, 3},  {BethesdaGame::GameType::ENDERAL, 5},
      {BethesdaGame::GameType::ENDERAL_SE, 6}, {BethesdaGame::GameType::SKYRIM_GOG, 7}};

  libInitialize(MutagenGameTypeMap.at(Game.getGameType()), Game.getGameDataPath().wstring(), L"ParallaxGen.esp",
                Game.getActivePlugins());
}

void ParallaxGenPlugin::populateObjs() { libPopulateObjs(); }

void ParallaxGenPlugin::processShape(const NIFUtil::ShapeShader &AppliedShader, const wstring &NIFPath,
                                     const wstring &Name3D, const int &Index3D,
                                     const PatcherUtil::PatcherObjectSet &Patchers,
                                     const unordered_map<wstring, int> *ModPriority,
                                     std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) {
  if (AppliedShader == NIFUtil::ShapeShader::NONE) {
    spdlog::trace(L"Plugin Patching | {} | {} | {} | Skipping: No shader applied", NIFPath, Name3D, Index3D);
    return;
  }

  lock_guard<mutex> Lock(ProcessShapeMutex);

  const auto Matches = libGetMatchingTXSTObjs(NIFPath, Name3D, Index3D);

  // loop through matches
  for (const auto &[TXSTIndex, AltTexIndex] : Matches) {
    int TXSTId = 0;
    bool NewTXST = false;
    // Check if the TXST is already modified
    {
      lock_guard<mutex> Lock(TXSTModMapMutex);

      if (TXSTModMap.find(TXSTIndex) != TXSTModMap.end()) {
        // TXST set was already patched, check to see which shader
        if (TXSTModMap[TXSTIndex].find(AppliedShader) == TXSTModMap[TXSTIndex].end()) {
          // TXST was patched, but not for the current shader. We need to make a new TXST record
          NewTXST = true;
          spdlog::trace(L"Plugin Patching | {} | {} | {} | New TXST record needed", NIFPath, Name3D, Index3D);
        } else {
          // TXST was patched, and for the current shader. We need to determine if AltTex is set correctly
          spdlog::trace(L"Plugin Patching | {} | {} | {} | TXST record already patched correctly", NIFPath, Name3D,
                        Index3D);
          TXSTId = TXSTModMap[TXSTIndex][AppliedShader];
          if (TXSTId != TXSTIndex) {
            // We need to set it
            spdlog::trace(L"Plugin Patching | {} | {} | {} | Setting alternate texture ID", NIFPath, Name3D, Index3D);
            libSetModelAltTex(AltTexIndex, TXSTId);
          }
          continue;
        }
      }
    }

    // Get TXST slots
    auto OldSlots = libGetTXSTSlots(TXSTIndex);
    auto BaseSlots = OldSlots;
    for (auto &Slot : BaseSlots) {
      // Remove PBR from beginning if its there so that we can actually process it
      if (boost::istarts_with(Slot, L"textures\\pbr\\")) {
        Slot.replace(0, 13, L"textures\\");
      }
    }

    // create a list of mods and the corresponding matches
    vector<PatcherUtil::ShaderPatcherMatch> Matches;
    for (const auto &[Shader, Patcher] : Patchers.ShaderPatchers) {
      Logger::Prefix PrefixPatches(ParallaxGenUtil::strToWstr(Patcher->getPatcherName()));

      // Check if shader should be applied
      vector<PatcherShader::PatcherMatch> CurMatches;
      if (!Patcher->shouldApply(BaseSlots, CurMatches)) {
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
          // check if transform is available
          if (AvailableTransforms.contains(AppliedShader)) {
            const auto TransformIter = AvailableTransforms.find(AppliedShader);
            CurMatch.ShaderTransformTo = TransformIter->first;
          }
        }

        // Add to matches if shader or transform matches applied shader
        if (Shader == AppliedShader || CurMatch.ShaderTransformTo == AppliedShader) {
          Matches.push_back(CurMatch);
        }
      }
    }

    // Find winning mod
    auto WinningShaderMatch = PatcherUtil::getWinningMatch(Matches, NIFPath, ModPriority);
    // Apply transforms
    WinningShaderMatch = PatcherUtil::applyTransformIfNeeded(WinningShaderMatch, Patchers);

    // Run patcher
    if (WinningShaderMatch.Match.MatchedPath.empty()) {
      // no shaders to apply
      lock_guard<mutex> Lock(TXSTWarningMapMutex);

      if (TXSTWarningMap.find(TXSTIndex) != TXSTWarningMap.end() && TXSTWarningMap[TXSTIndex] == AppliedShader) {
        // Warning already issued
      } else {
        TXSTWarningMap[TXSTIndex] = AppliedShader;
        const auto [FormID, PluginName] = libGetTXSTFormID(TXSTIndex);

        // todo: implement ESL flagged
        const bool bESLFlagged = false;
        const wstring FormIDStr =
            (!bESLFlagged) ? format(L"{}/{:06X}", PluginName, FormID) : format(L"{0}/{1:03X}", PluginName, FormID);
        spdlog::warn(L"Did not find required {} textures for {}, TXST {} - setting neutral textures",
                     ParallaxGenUtil::strToWstr(NIFUtil::getStrFromShader(AppliedShader)),
                     BaseSlots[static_cast<int>(NIFUtil::TextureSlots::DIFFUSE)], FormIDStr);
      }

      NewSlots = Patchers.ShaderPatchers.at(AppliedShader)->applyNeutral(BaseSlots);
    } else {
      NewSlots = Patchers.ShaderPatchers.at(AppliedShader)->applyPatchSlots(BaseSlots, WinningShaderMatch.Match);
    }

    // Post warnings if any
    for (const auto &CurMatchedFrom : WinningShaderMatch.Match.MatchedFrom) {
      ParallaxGenWarnings::mismatchWarn(WinningShaderMatch.Match.MatchedPath,
                                        NewSlots[static_cast<int>(CurMatchedFrom)]);
    }

    ParallaxGenWarnings::meshWarn(WinningShaderMatch.Match.MatchedPath, NIFPath);

    // Check if oldprefix is the same as newprefix
    bool FoundDiff = false;
    for (int I = 0; I < NUM_TEXTURE_SLOTS; ++I) {
      if (!boost::iequals(OldSlots[I], NewSlots[I])) {
        FoundDiff = true;
      }
    }

    if (!FoundDiff) {
      // No need to patch
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Not patching because nothing to change", NIFPath, Name3D,
                    Index3D);
      continue;
    }

    // Patch record
    if (NewTXST) {
      // Create a new TXST record
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Creating a new TXST record and patching", NIFPath, Name3D,
                    Index3D);
      TXSTId = libCreateNewTXSTPatch(AltTexIndex, NewSlots);
      {
        lock_guard<mutex> Lock(TXSTModMapMutex);
        TXSTModMap[TXSTIndex][AppliedShader] = TXSTId;
      }
    } else {
      // Update the existing TXST record
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Patching an existing TXST record", NIFPath, Name3D, Index3D);
      libCreateTXSTPatch(TXSTIndex, NewSlots);
      {
        lock_guard<mutex> Lock(TXSTModMapMutex);
        TXSTModMap[TXSTIndex][AppliedShader] = TXSTIndex;
      }
    }
  }
}

void ParallaxGenPlugin::set3DIndices(const wstring &NIFPath, const vector<uint32_t> &SortOrder,
                                     const unordered_map<uint32_t, wstring> &ShapeBlocks,
                                     const unordered_set<int> &DeletedIndex3Ds) {
  // Build new shape vector
  // 1. Index3D without considering deletions
  // 2. Index3D with deletions
  // 3. Original block id
  vector<tuple<int, int, uint32_t, wstring>> OldShape3DIndex(ShapeBlocks.size());
  int I = 0;
  int IDel = 0;
  for (const auto &OldIndex : SortOrder) {
    if (!ShapeBlocks.contains(OldIndex)) {
      // current block id is not a shape
      continue;
    }

    // skip any deleted indices
    while (DeletedIndex3Ds.contains(IDel)) {
      IDel++;
    }

    OldShape3DIndex.emplace_back(I, IDel, OldIndex, ShapeBlocks.at(OldIndex));
    I++;
    IDel++;
  }

  // Sort vector by old index
  auto NewShape3DIndex = OldShape3DIndex;
  sort(NewShape3DIndex.begin(), NewShape3DIndex.end(),
       [](const tuple<int, int, uint32_t, wstring> &A, const tuple<int, int, uint32_t, wstring> &B) { return get<2>(A) < get<2>(B); });

  // Create map
  unordered_map<int, pair<int, wstring>> Index3DUpdateMap;
  for (int I = 0; I < NewShape3DIndex.size(); ++I) {
    Logger::trace(L"Mapping old blockid shape {} to new blockid shape {}", get<2>(OldShape3DIndex[I]), get<2>(NewShape3DIndex[I]));
    Index3DUpdateMap[get<1>(OldShape3DIndex[I])] = {get<0>(NewShape3DIndex[I]), get<3>(NewShape3DIndex[I])};
  }

  // Loop through map
  for (const auto &[OldIndex, NewIndex] : Index3DUpdateMap) {
    // find matches
    const auto Matches = libGetMatchingTXSTObjs(NIFPath, NewIndex.second, OldIndex);

    // Set indices
    for (const auto &[TXSTIndex, AltTexIndex] : Matches) {
      if (NewIndex.first == OldIndex) {
        // No change
        continue;
      }

      libSet3DIndex(AltTexIndex, NewIndex.first);
    }
  }
}

void ParallaxGenPlugin::savePlugin(const filesystem::path &OutputDir) { libFinalize(OutputDir); }
