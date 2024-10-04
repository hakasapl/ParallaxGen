#include "ParallaxGenPlugin.hpp"

#include <fstream>
#include <mutex>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <winbase.h>

#include "NIFUtil.hpp"
#include "ParallaxGenMutagenWrapperNE.h"
#include "ParallaxGenUtil.hpp"
#include "Patchers/PatcherComplexMaterial.hpp"
#include "Patchers/PatcherTruePBR.hpp"
#include "Patchers/PatcherVanillaParallax.hpp"

using namespace std;

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
  std::vector<const wchar_t *> LoadOrderArr;
  if (!LoadOrder.empty()) {
    LoadOrderArr.reserve(LoadOrder.size()); // Pre-allocate the vector size
    for (const auto &Mod : LoadOrder) {
      LoadOrderArr.push_back(Mod.c_str()); // Populate the vector with the c_str pointers
    }
  }

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
  GetMatchingTXSTObjsLength(NIFName.c_str(), Name3D.c_str(), Index3D, &Length);
  libLogMessageIfExists();
  libThrowExceptionIfExists();

  vector<int> TXSTIdArray(Length);
  vector<int> AltTexIdArray(Length);
  GetMatchingTXSTObjs(NIFName.c_str(), Name3D.c_str(), Index3D, TXSTIdArray.data(), AltTexIdArray.data());
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

void ParallaxGenPlugin::initialize(const BethesdaGame &Game) {
  // Maps BethesdaGame::GameType to Mutagen game type
  static const unordered_map<BethesdaGame::GameType, int> MutagenGameTypeMap = {
      {BethesdaGame::GameType::SKYRIM, 1},     {BethesdaGame::GameType::SKYRIM_SE, 2},
      {BethesdaGame::GameType::SKYRIM_VR, 3},  {BethesdaGame::GameType::ENDERAL, 5},
      {BethesdaGame::GameType::ENDERAL_SE, 6}, {BethesdaGame::GameType::SKYRIM_GOG, 7}};

  // Create load order vector
  vector<wstring> LoadOrder;
  auto LoadOrderFile = Game.getLoadOrderFile();
  // open file
  wifstream LoadOrderStream(LoadOrderFile);
  if (LoadOrderStream.is_open()) {
    wstring Line;
    while (getline(LoadOrderStream, Line)) {
      if (Line.empty() || Line.starts_with(L"#")) {
        // skip empty lines and comments
        continue;
      }

      LoadOrder.push_back(Line);
    }
    LoadOrderStream.close();
  }

  libInitialize(MutagenGameTypeMap.at(Game.getGameType()), Game.getGameDataPath().wstring(), L"ParallaxGen.esp", LoadOrder);
}

void ParallaxGenPlugin::populateObjs() { libPopulateObjs(); }

void ParallaxGenPlugin::processShape(const NIFUtil::ShapeShader &AppliedShader, const wstring &NIFPath,
                                     const wstring &Name3D, const int &Index3DOld, const int &Index3DNew) {
  if (AppliedShader == NIFUtil::ShapeShader::NONE) {
    spdlog::trace(L"Plugin Patching | {} | {} | {} | Skipping: No shader applied", NIFPath, Name3D, Index3DOld);
    return;
  }

  auto Matches = libGetMatchingTXSTObjs(NIFPath, Name3D, Index3DOld);

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
          spdlog::trace(L"Plugin Patching | {} | {} | {} | New TXST record needed", NIFPath, Name3D, Index3DOld);
        } else {
          // TXST was patched, and for the current shader. We need to determine if AltTex is set correctly
          spdlog::trace(L"Plugin Patching | {} | {} | {} | TXST record already patched correctly", NIFPath, Name3D,
                        Index3DOld);
          TXSTId = TXSTModMap[TXSTIndex][AppliedShader];
          if (TXSTId != TXSTIndex) {
            // We need to set it
            spdlog::trace(L"Plugin Patching | {} | {} | {} | Setting alternate texture ID", NIFPath, Name3D,
                          Index3DOld);
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

    auto SearchPrefixes = NIFUtil::getSearchPrefixes(BaseSlots);
    array<wstring, NUM_TEXTURE_SLOTS> NewSlots;

    // Attempt to patch the relevant shader
    bool PatchTXST = false;
    string ShaderLabel;
    if (AppliedShader == NIFUtil::ShapeShader::VANILLAPARALLAX) {
      ShaderLabel = "Parallax";
      wstring MatchedPath;
      bool ShouldApply = PatcherVanillaParallax::shouldApplySlots(SearchPrefixes, MatchedPath);

      if (ShouldApply) {
        NewSlots = PatcherVanillaParallax::applyPatchSlots(BaseSlots, MatchedPath);
        PatchTXST = true;
      }
    } else if (AppliedShader == NIFUtil::ShapeShader::COMPLEXMATERIAL) {
      ShaderLabel = "Complex Material";
      wstring MatchedPath;
      bool EnableDynCubemaps = false;
      bool ShouldApply =
          PatcherComplexMaterial::shouldApplySlots(SearchPrefixes, MatchedPath, EnableDynCubemaps, NIFPath);

      if (ShouldApply) {
        NewSlots = PatcherComplexMaterial::applyPatchSlots(BaseSlots, MatchedPath, EnableDynCubemaps);
        PatchTXST = true;
      }
    } else if (AppliedShader == NIFUtil::ShapeShader::TRUEPBR) {
      ShaderLabel = "TruePBR";
      map<size_t, tuple<nlohmann::json, wstring>> TruePBRData;
      bool ShouldApply = PatcherTruePBR::shouldApplySlots(L"Plugin Patching | ", SearchPrefixes, NIFPath, TruePBRData);

      if (ShouldApply) {
        auto TempSlots = BaseSlots;
        for (auto &TruePBRCFG : TruePBRData) {
          TempSlots = PatcherTruePBR::applyPatchSlots(TempSlots, get<0>(TruePBRCFG.second), get<1>(TruePBRCFG.second));
        }

        NewSlots = TempSlots;
        PatchTXST = true;
      }
    } else {
      continue;
    }

    // Check if TXST was able to be patched
    if (!PatchTXST) {
      lock_guard<mutex> Lock(TXSTWarningMapMutex);

      if (TXSTWarningMap.find(TXSTIndex) != TXSTWarningMap.end() && TXSTWarningMap[TXSTIndex] == AppliedShader) {
        // Warning already issued
        continue;
      }

      TXSTWarningMap[TXSTIndex] = AppliedShader;
      const auto [FormID, PluginName] = libGetTXSTFormID(TXSTIndex);
      spdlog::warn(L"TXST record is not able to patched for {}: {} / 0x{:06X}", ParallaxGenUtil::strToWstr(ShaderLabel),
                   PluginName, FormID);
      continue;
    }

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
                    Index3DOld);
      continue;
    }

    // Patch record
    if (NewTXST) {
      // Create a new TXST record
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Creating a new TXST record and patching", NIFPath, Name3D,
                    Index3DOld);
      TXSTId = libCreateNewTXSTPatch(AltTexIndex, NewSlots);
      {
        lock_guard<mutex> Lock(TXSTModMapMutex);
        TXSTModMap[TXSTIndex][AppliedShader] = TXSTId;
      }
    } else {
      // Update the existing TXST record
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Patching an existing TXST record", NIFPath, Name3D, Index3DOld);
      libCreateTXSTPatch(TXSTIndex, NewSlots);
      {
        lock_guard<mutex> Lock(TXSTModMapMutex);
        TXSTModMap[TXSTIndex][AppliedShader] = TXSTIndex;
      }
    }

    // Check if 3d index needs to be patched
    if (Index3DNew != Index3DOld) {
      spdlog::trace(L"Plugin Patching | {} | {} | {} | Setting 3D index due to shape deletion", NIFPath, Name3D,
                    Index3DOld);
      libSet3DIndex(AltTexIndex, Index3DNew);
    }
  }
}

void ParallaxGenPlugin::savePlugin(const filesystem::path &OutputDir) { libFinalize(OutputDir); }
