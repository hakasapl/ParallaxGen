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

  throw runtime_error("ParallaxGenMutagenWrapper.dll: " + ParallaxGenUtil::UTF16toASCII(MessageOut));
}

void ParallaxGenPlugin::libInitialize(const int &GameType, const wstring &DataPath, const wstring &OutputPlugin,
                                      const vector<wstring> &LoadOrder) {
  lock_guard<mutex> Lock(LibMutex);

  // Use vector to manage the memory for LoadOrderArr
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

void ParallaxGenPlugin::libFinalize(const filesystem::path &OutputPath, const bool &ESMify) {
  lock_guard<mutex> Lock(LibMutex);

  Finalize(OutputPath.c_str(), static_cast<int>(ESMify));
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
                                              const array<wstring, NUM_TEXTURE_SLOTS> &Slots, const string &NewEDID) -> int {
  lock_guard<mutex> Lock(LibMutex);

  // Prepare the array of const wchar_t* pointers from the Slots array
  const wchar_t *SlotsArray[NUM_TEXTURE_SLOTS] = {nullptr};
  for (int I = 0; I < NUM_TEXTURE_SLOTS; ++I) {
    SlotsArray[I] = Slots[I].c_str(); // Point to the internal wide string data of each wstring
  }

  // Call the CreateNewTXSTPatch function with AltTexIndex and the array of wide string pointers
  int NewTXSTId = 0;
  CreateNewTXSTPatch(AltTexIndex, SlotsArray, NewEDID.c_str(), &NewTXSTId);
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

auto ParallaxGenPlugin::libGetModelRecHandleFromAltTexHandle(const int &AltTexIndex) -> int {
  lock_guard<mutex> Lock(LibMutex);

  int ModelRecHandle = 0;
  GetModelRecHandleFromAltTexHandle(AltTexIndex, &ModelRecHandle);
  libLogMessageIfExists();
  libThrowExceptionIfExists();

  return ModelRecHandle;
}

void ParallaxGenPlugin::libSetModelRecNIF(const int &ModelRecHandle, const wstring &NIFPath) {
  lock_guard<mutex> Lock(LibMutex);

  SetModelRecNIF(ModelRecHandle, NIFPath.c_str());
  libLogMessageIfExists();
  libThrowExceptionIfExists();
}

// Statics
mutex ParallaxGenPlugin::CreatedTXSTMutex;
unordered_map<array<wstring, NUM_TEXTURE_SLOTS>, int, ParallaxGenPlugin::ArrayHash, ParallaxGenPlugin::ArrayEqual>
    ParallaxGenPlugin::CreatedTXSTs;

mutex ParallaxGenPlugin::EDIDCounterMutex;
int ParallaxGenPlugin::EDIDCounter = 0;

ParallaxGenDirectory *ParallaxGenPlugin::PGD;

mutex ParallaxGenPlugin::ProcessShapeMutex;

void ParallaxGenPlugin::loadStatics(ParallaxGenDirectory *PGD) { ParallaxGenPlugin::PGD = PGD; }

unordered_map<wstring, int> *ParallaxGenPlugin::ModPriority;

void ParallaxGenPlugin::loadModPriorityMap(std::unordered_map<std::wstring, int> *ModPriority) {
  ParallaxGenPlugin::ModPriority = ModPriority;
}

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

void ParallaxGenPlugin::processShape(const wstring &NIFPath, nifly::NiShape *NIFShape, const wstring &Name3D,
                                     const int &Index3D, PatcherUtil::PatcherObjectSet &Patchers,
                                     vector<TXSTResult> &Results, PatcherUtil::ConflictModResults *ConflictMods) {
  lock_guard<mutex> Lock(ProcessShapeMutex);

  const auto Matches = libGetMatchingTXSTObjs(NIFPath, Name3D, Index3D);

  Results.clear();

  // loop through matches
  for (const auto &[TXSTIndex, AltTexIndex] : Matches) {
    // TODO! caching (central location)
    //  Allowed shaders from result of patchers
    vector<PatcherUtil::ShaderPatcherMatch> Matches;

    // Output information
    TXSTResult CurResult;

    // Get TXST slots
    auto OldSlots = libGetTXSTSlots(TXSTIndex);
    auto BaseSlots = OldSlots;
    for (auto &Slot : BaseSlots) {
      // Remove PBR from beginning if its there so that we can actually process it
      if (boost::istarts_with(Slot, L"textures\\pbr\\")) {
        Slot.replace(0, 13, L"textures\\");
      }
    }

    // Loop through each shader
    unordered_set<wstring> ModSet;
    for (const auto &[Shader, Patcher] : Patchers.ShaderPatchers) {
      // note: name is defined in source code in UTF8-encoded files
      Logger::Prefix PrefixPatches(ParallaxGenUtil::UTF8toUTF16(Patcher->getPatcherName()));

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

    if (Matches.empty()) {
      // no shaders to apply
      Logger::trace(L"Rejecting: No shaders to apply");
      return;
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

      return;
    }

    // Get winning match
    auto WinningShaderMatch = PatcherUtil::getWinningMatch(Matches, ModPriority);

    // Apply transforms
    WinningShaderMatch = PatcherUtil::applyTransformIfNeeded(WinningShaderMatch, Patchers);
    CurResult.Shader = WinningShaderMatch.Shader;

    // loop through patchers
    NIFUtil::TextureSet NewSlots =
        Patchers.ShaderPatchers.at(WinningShaderMatch.Shader)->applyPatchSlots(BaseSlots, WinningShaderMatch.Match);

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
        break;
      }
    }

    if (!FoundDiff) {
      // No need to patch
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Not patching because nothing to change", NIFPath, Name3D,
                    Index3D);
      continue;
    }

    CurResult.AltTexIndex = AltTexIndex;
    CurResult.ModelRecHandle = libGetModelRecHandleFromAltTexHandle(AltTexIndex);

    {
      lock_guard<mutex> Lock(CreatedTXSTMutex);

      // Check if we need to make a new TXST record
      if (CreatedTXSTs.contains(NewSlots)) {
        // Already modded
        spdlog::trace(L"Plugin Patching | {} | {} | {} | Already added, skipping", NIFPath, Name3D, Index3D);
        CurResult.TXSTIndex = CreatedTXSTs[NewSlots];
        Results.push_back(CurResult);
        continue;
      }

      // Create a new TXST record
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Creating a new TXST record and patching", NIFPath, Name3D,
                    Index3D);
      string NewEDID = fmt::format("PGTXST{:05d}", EDIDCounter++);
      CurResult.TXSTIndex = libCreateNewTXSTPatch(AltTexIndex, NewSlots, NewEDID);
      Patchers.ShaderPatchers.at(WinningShaderMatch.Shader)->processNewTXSTRecord(WinningShaderMatch.Match, NewEDID);
      CreatedTXSTs[NewSlots] = CurResult.TXSTIndex;
    }

    // add to result
    Results.push_back(CurResult);
  }
}

void ParallaxGenPlugin::assignMesh(const wstring &NIFPath, const vector<TXSTResult> &Result) {
  lock_guard<mutex> Lock(ProcessShapeMutex);

  Logger::Prefix Prefix(L"assignMesh");

  // Loop through results
  for (const auto &CurResult : Result) {
    // Set model rec handle
    libSetModelRecNIF(CurResult.AltTexIndex, NIFPath);

    // Set model alt tex
    libSetModelAltTex(CurResult.AltTexIndex, CurResult.TXSTIndex);
  }
}

void ParallaxGenPlugin::set3DIndices(const wstring &NIFPath,
                                     const vector<tuple<nifly::NiShape *, int, int>> &ShapeTracker) {
  lock_guard<mutex> Lock(ProcessShapeMutex);

  Logger::Prefix Prefix(L"set3DIndices");

  // Loop through shape tracker
  for (const auto &[Shape, OldIndex3D, NewIndex3D] : ShapeTracker) {
    const auto ShapeName = ParallaxGenUtil::ASCIItoUTF16(Shape->name.get());

    // find matches
    const auto Matches = libGetMatchingTXSTObjs(NIFPath, ShapeName, OldIndex3D);

    // Set indices
    for (const auto &[TXSTIndex, AltTexIndex] : Matches) {
      if (OldIndex3D == NewIndex3D) {
        // No change
        continue;
      }

      Logger::trace(L"Setting 3D index for AltTex {} to {}", AltTexIndex, NewIndex3D);
      libSet3DIndex(AltTexIndex, NewIndex3D);
    }
  }
}

void ParallaxGenPlugin::savePlugin(const filesystem::path &OutputDir, bool ESMify) { libFinalize(OutputDir, ESMify); }
