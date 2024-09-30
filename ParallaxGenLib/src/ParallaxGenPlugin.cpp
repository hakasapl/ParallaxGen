#include "ParallaxGenPlugin.hpp"

#include <spdlog/spdlog.h>
#include <unordered_map>

#include "NIFUtil.hpp"
#include "ParallaxGenMutagenWrapperNE.h"
#include "ParallaxGenUtil.hpp"
#include "Patchers/PatcherComplexMaterial.hpp"
#include "Patchers/PatcherTruePBR.hpp"
#include "Patchers/PatcherVanillaParallax.hpp"

using namespace std;

mutex ParallaxGenPlugin::LibMutex;

void ParallaxGenPlugin::libThrowExceptionIfExists() {
  int Length = 0;
  GetLastExceptionLength(&Length);
  if (Length > 0) {
    unique_ptr<wchar_t[]> ErrMessage = make_unique<wchar_t[]>(Length + 1);
    GetLastException(ErrMessage.get());
    wstring ErrMessageOut(ErrMessage.get(), Length);
    throw runtime_error("ParallaxGenMutagenWrapper.dll: " + ParallaxGenUtil::wstrToStr(ErrMessageOut));
  }
}

void ParallaxGenPlugin::libInitialize(const int &GameType, const wstring &DataPath, const wstring &OutputPlugin) {
  lock_guard<mutex> Lock(LibMutex);

  Initialize(GameType, DataPath.c_str(), OutputPlugin.c_str());
  libThrowExceptionIfExists();
}

void ParallaxGenPlugin::libPopulateObjs() {
  lock_guard<mutex> Lock(LibMutex);

  PopulateObjs();
  libThrowExceptionIfExists();
}

void ParallaxGenPlugin::libFinalize(const filesystem::path &OutputPath) {
  lock_guard<mutex> Lock(LibMutex);

  Finalize(OutputPath.c_str());
  libThrowExceptionIfExists();
}

auto ParallaxGenPlugin::libGetMatchingTXSTObjs(const wstring &NIFName, const wstring &Name3D,
                                               const int &Index3D) -> vector<tuple<int, int>> {
  lock_guard<mutex> Lock(LibMutex);

  int Length = 0;
  GetMatchingTXSTObjsLength(NIFName.c_str(), Name3D.c_str(), Index3D, &Length);
  libThrowExceptionIfExists();

  vector<int> TXSTIdArray(Length);
  vector<int> AltTexIdArray(Length);
  GetMatchingTXSTObjs(NIFName.c_str(), Name3D.c_str(), Index3D, TXSTIdArray.data(), AltTexIdArray.data());
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
  libThrowExceptionIfExists();

  // Process the slots
  for (int I = 0; I < NUM_TEXTURE_SLOTS; ++I) {
    if (SlotsArray[I] != nullptr) {
      // Convert to C-style string (Ansi)
      const auto *SlotStr = static_cast<const wchar_t *>(SlotsArray[I]);
      OutputArray[I] = SlotStr;

      // Free the unmanaged memory allocated by Marshal.StringToHGlobalAnsi
      GlobalFree(static_cast<HGLOBAL>(SlotsArray[I]));
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
  libThrowExceptionIfExists();

  return NewTXSTId;
}

void ParallaxGenPlugin::libSetModelAltTex(const int &AltTexIndex, const int &TXSTIndex) {
  lock_guard<mutex> Lock(LibMutex);

  SetModelAltTex(AltTexIndex, TXSTIndex);
  libThrowExceptionIfExists();
}

void ParallaxGenPlugin::libSet3DIndex(const int &AltTexIndex, const int &Index3D) {
  lock_guard<mutex> Lock(LibMutex);

  Set3DIndex(AltTexIndex, Index3D);
  libThrowExceptionIfExists();
}

auto ParallaxGenPlugin::libGetTXSTFormID(const int &TXSTIndex) -> unsigned int {
  lock_guard<mutex> Lock(LibMutex);

  unsigned int FormID = 0;
  GetTXSTFormID(TXSTIndex, &FormID);
  libThrowExceptionIfExists();

  return FormID;
}

mutex ParallaxGenPlugin::TXSTModMapMutex;
unordered_map<int, unordered_map<NIFUtil::ShapeShader, int>> ParallaxGenPlugin::TXSTModMap;  // NOLINT

void ParallaxGenPlugin::initialize(const BethesdaGame &Game) {
  static const unordered_map<BethesdaGame::GameType, int> MutagenGameTypeMap = {
      {BethesdaGame::GameType::SKYRIM, 1},     {BethesdaGame::GameType::SKYRIM_SE, 2},
      {BethesdaGame::GameType::SKYRIM_VR, 3},  {BethesdaGame::GameType::ENDERAL, 5},
      {BethesdaGame::GameType::ENDERAL_SE, 6}, {BethesdaGame::GameType::SKYRIM_GOG, 7}};

  libInitialize(MutagenGameTypeMap.at(Game.getGameType()), Game.getGameDataPath().wstring(), L"ParallaxGen.esp");
}

void ParallaxGenPlugin::populateObjs() { libPopulateObjs(); }

void ParallaxGenPlugin::processShape(const NIFUtil::ShapeShader &AppliedShader, const wstring &NIFPath,
                                     const wstring &Name3D, const int &Index3DOld, const int &Index3DNew) {
  if (AppliedShader == NIFUtil::ShapeShader::NONE) {
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
        } else {
          // TXST was patched, and for the current shader. We need to determine if AltTex is set correctly
          TXSTId = TXSTModMap[TXSTIndex][AppliedShader];
          if (TXSTId != TXSTIndex) {
            // We need to set it
            libSetModelAltTex(AltTexIndex, TXSTId);
          }
          continue;
        }
      }
    }

    // Get TXST slots
    auto OldSlots = libGetTXSTSlots(TXSTIndex);
    auto SearchPrefixes = NIFUtil::getSearchPrefixes(OldSlots);
    array<wstring, NUM_TEXTURE_SLOTS> NewSlots;

    // Attempt to patch the relevant shader
    bool PatchTXST = false;
    string ShaderLabel;
    if (AppliedShader == NIFUtil::ShapeShader::VANILLAPARALLAX) {
      ShaderLabel = "Parallax";
      wstring MatchedPath;
      bool ShouldApply = PatcherVanillaParallax::shouldApplySlots(SearchPrefixes, MatchedPath);

      if (ShouldApply) {
        NewSlots = PatcherVanillaParallax::applyPatchSlots(OldSlots, MatchedPath);
        PatchTXST = true;
      }
    } else if (AppliedShader == NIFUtil::ShapeShader::COMPLEXMATERIAL) {
      ShaderLabel = "Complex Material";
      wstring MatchedPath;
      bool EnableDynCubemaps = false;
      bool ShouldApply =
          PatcherComplexMaterial::shouldApplySlots(SearchPrefixes, MatchedPath, EnableDynCubemaps, NIFPath);

      if (ShouldApply) {
        NewSlots = PatcherComplexMaterial::applyPatchSlots(OldSlots, MatchedPath, EnableDynCubemaps);
        PatchTXST = true;
      }
    } else if (AppliedShader == NIFUtil::ShapeShader::TRUEPBR) {
      ShaderLabel = "True PBR";
      map<size_t, tuple<nlohmann::json, wstring>> TruePBRData;
      bool ShouldApply = PatcherTruePBR::shouldApplySlots(L"Plugin Patching | ", SearchPrefixes, NIFPath, TruePBRData);

      if (ShouldApply) {
        for (auto &TruePBRCFG : TruePBRData) {
          OldSlots = PatcherTruePBR::applyPatchSlots(OldSlots, get<0>(TruePBRCFG.second), get<1>(TruePBRCFG.second));
        }

        NewSlots = OldSlots;
        PatchTXST = true;
      }
    } else {
      continue;
    }

    // Check if TXST was able to be patched
    if (!PatchTXST) {
      // TODO only show one warning per TXST record
      spdlog::warn("TXST record with Form ID {} is not able to be patched with {}. This will cause issues",
                   libGetTXSTFormID(TXSTIndex), ShaderLabel);
      continue;
    }

    // Check if oldprefix is the same as newprefix
    if (OldSlots == NewSlots) {
      // No need to patch
      continue;
    }

    // Patch record
    if (NewTXST) {
      // Create a new TXST record
      TXSTId = libCreateNewTXSTPatch(AltTexIndex, NewSlots);
      {
        lock_guard<mutex> Lock(TXSTModMapMutex);
        TXSTModMap[TXSTIndex][AppliedShader] = TXSTId;
      }
    } else {
      // Update the existing TXST record
      libCreateTXSTPatch(TXSTIndex, NewSlots);
      {
        lock_guard<mutex> Lock(TXSTModMapMutex);
        TXSTModMap[TXSTIndex][AppliedShader] = TXSTIndex;
      }
    }

    // Check if 3d index needs to be patched
    if (Index3DNew != Index3DOld) {
      libSet3DIndex(AltTexIndex, Index3DNew);
    }
  }
}

void ParallaxGenPlugin::savePlugin(const filesystem::path &OutputDir) {
  libFinalize(OutputDir);
}