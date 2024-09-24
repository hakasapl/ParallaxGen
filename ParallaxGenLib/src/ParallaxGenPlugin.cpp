#include "BethesdaGame.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenTask.hpp"
#include "XEditLibCpp.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <windows.h>

#include "Patchers/PatcherComplexMaterial.hpp"
#include "Patchers/PatcherTruePBR.hpp"
#include "Patchers/PatcherVanillaParallax.hpp"


using namespace std;

ParallaxGenPlugin::ParallaxGenPlugin(const BethesdaGame &Game) {
  // Convert game type
  XEditLibCpp::GameMode Mode = XEditLibCpp::GameMode::SkyrimSE;
  switch (Game.getGameType()) {
  case BethesdaGame::GameType::ENDERAL_SE:
  case BethesdaGame::GameType::SKYRIM_SE:
  case BethesdaGame::GameType::SKYRIM_VR:
  case BethesdaGame::GameType::SKYRIM_GOG:
    Mode = XEditLibCpp::GameMode::SkyrimSE;
    break;
  case BethesdaGame::GameType::SKYRIM:
  case BethesdaGame::GameType::ENDERAL:
    Mode = XEditLibCpp::GameMode::Skyrim;
    break;
  }

  // Set XeditLib Params
  setGameMode(Mode);
  setGamePath(Game.getGamePath());

  // Get Plugins
  ActivePlugins = getActivePlugins();
}

void ParallaxGenPlugin::initThread() {
  boost::thread(&ParallaxGenPlugin::init, this);
}

auto ParallaxGenPlugin::init() -> ParallaxGenTask::PGResult {
  // Load Plugins
  loadPlugins(ActivePlugins, false);

  auto LoaderStatus = getLoaderStatus();
  while (LoaderStatus != XEditLibCpp::LoaderStates::IsDone && LoaderStatus != XEditLibCpp::LoaderStates::IsError) {
    LoaderStatus = getLoaderStatus();
    Sleep(PLUGIN_LOADER_CHECK_INTERVAL);
  }

  if (LoaderStatus == XEditLibCpp::LoaderStates::IsError) {
    spdlog::critical("Plugin loading failed");
    InitResult = 2;
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // Build references
  buildReferences(0, true);

  // Get TXST objects
  auto TXSTs = getRecords(0, L"TXST", true);

  // Find references to each TXST
  for (const auto &TXST : TXSTs) {
    auto TXSTFormID = getFormID(TXST, false);
    auto TXSTFormIDHex = fmt::format("{:X}", TXSTFormID);
    spdlog::trace("Plugin Patching Init | Found Texture Set | {}", TXSTFormID);

    // Find references to the TXST
    auto CurTXSTRefs = getReferencedBy(TXST);
    // loop through each reference
    for (const auto &Ref : CurTXSTRefs) {
      // Get the form id of the reference
      auto FormID = getFormID(Ref, false);
      spdlog::trace("Plugin Patching Init | Found Reference | {} | {}", TXSTFormID, FormID);

      // Check for alternate textures for each type
      if (hasElement(Ref, L"MODL") && hasElement(Ref, L"MODL\\MODS")) {
        addAlternateTexturesToMap(getElement(Ref, L"MODL"), L"MODL", L"MODS", TXSTFormIDHex, TXST);
      }

      if (hasElement(Ref, L"MOD2") && hasElement(Ref, L"MOD2\\MO2S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD2"), L"MOD2", L"MO2S", TXSTFormIDHex, TXST);
      }

      if (hasElement(Ref, L"MOD3") && hasElement(Ref, L"MOD3\\MO3S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD3"), L"MOD3", L"MO3S", TXSTFormIDHex, TXST);
      }

      if (hasElement(Ref, L"MOD4") && hasElement(Ref, L"MOD4\\MO4S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD4"), L"MOD4", L"MO4S", TXSTFormIDHex, TXST);
      }

      if (hasElement(Ref, L"MOD5") && hasElement(Ref, L"MOD5\\MO5S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD5"), L"MOD5", L"MO5S", TXSTFormIDHex, TXST);
      }
    }
  }

  InitResult = 1;
  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenPlugin::isInitDone() const -> bool {
  return InitResult != 0;
}

auto ParallaxGenPlugin::getInitResult() const -> int { return InitResult; }

void ParallaxGenPlugin::addAlternateTexturesToMap(const unsigned int &Id, const std::wstring &MDLKey,
                                                  const std::wstring &AltTXSTKey, const string &TXSTFormIDHex,
                                                  const unsigned int &TXSTId) {
  // Get the NIF name
  auto NIFName = getValue(Id, MDLKey);
  boost::to_lower(NIFName); // Lowercase nif name

  // Loop through alternate textures
  auto AltTXSTs = getElements(Id, AltTXSTKey, false, false);
  for (const auto &AltTXST : AltTXSTs) {
    // Get 3D Name
    auto Name3D = getValue(AltTXST, L"3D Name");

    // Get 3D Index
    auto Index3D = getIntValue(AltTXST, L"3D Index");

    // Get New TXST
    auto NewTXST = getValue(AltTXST, L"New Texture");
    wstring TXSTTestString = L"TXST";
    if (boost::icontains(NewTXST, TXSTFormIDHex)) {
      // this is the correct TXST
      TXSTRefID KeyVal = {NIFName, Name3D, Index3D};
      TXSTRefsMap[KeyVal].insert(TXSTId);
    }
  }
}

auto ParallaxGenPlugin::getOutputPluginName() -> filesystem::path {
  static filesystem::path OutputPlugin = "ParallaxGen.esp";
  return OutputPlugin;
}

auto ParallaxGenPlugin::createPlugin(
    const std::filesystem::path &OutputDir,
    const std::unordered_map<ParallaxGenPlugin::TXSTRefID, NIFUtil::ShapeShader,
                             ParallaxGenPlugin::TXSTRefIDHash> &MeshTXSTMap) -> ParallaxGenTask::PGResult {
  spdlog::info("Creating ParallaxGen.esp Plugin");

  // Add file
  auto PluginHandler = addFile(getOutputPluginName());

  // Loop through each TXSTRefID (all found TXST references)
  for (const auto &[Key, TXSTIds] : TXSTRefsMap) {
    // check if Key is in MeshTXSTMap
    if (MeshTXSTMap.find(Key) == MeshTXSTMap.end()) {
      // This TXST set was not patched during mesh patching, continue
      continue;
    }

    auto ShaderApplied = MeshTXSTMap.at(Key);
    if (ShaderApplied == NIFUtil::ShapeShader::NONE) {
      // No shader was applied
      continue;
    }

    // Loop through items in handler set
    for (const auto &TXSTId : TXSTIds) {
      auto TXSTFormID = getFormID(TXSTId, false);
      if (PatchedTXSTTracker.find(TXSTFormID) != PatchedTXSTTracker.end()) {
        // This TXST was already patched
        // TODO how do we deal with multiple shaders fighting for one TXST record? Make new ones?
        continue;
      }
      PatchedTXSTTracker.insert(TXSTFormID);

      // Get existing texture set
      array<wstring, NUM_TEXTURE_SLOTS> OldSlots;
      for (int I = 0; I < NUM_TEXTURE_SLOTS - 1; I++) {
        wstring PluginTexSlot = L"TX0" + to_wstring(I);
        wstring KeyVal = L"Textures (RGB/A)\\" + PluginTexSlot;
        if (hasElement(TXSTId, KeyVal)) {
          auto NIFSlot = getNifSlotFromPluginSlot(PluginTexSlot);
          OldSlots[static_cast<size_t>(NIFSlot)] = boost::to_lower_copy(L"textures\\" + getValue(TXSTId, KeyVal));
        }
      }

      auto SearchPrefixes = NIFUtil::getSearchPrefixes(OldSlots);

      array<wstring, NUM_TEXTURE_SLOTS> NewSlots;

      bool PatchTXST = false;

      wstring MatchedPath;
      bool ShouldApply = false;
      bool EnableDynCubemaps = false;
      switch(ShaderApplied) {  // NOLINT
        case NIFUtil::ShapeShader::VANILLAPARALLAX:
          MatchedPath = L"";
          ShouldApply = PatcherVanillaParallax::shouldApplySlots(SearchPrefixes, MatchedPath);

          if (ShouldApply) {
            NewSlots = PatcherVanillaParallax::applyPatchSlots(OldSlots, MatchedPath);
            PatchTXST = true;
          } else {
            spdlog::warn("TXST record with Form ID {} is not able to be patched with Vanilla Parallax, but the mesh was. This will cause issues", TXSTFormID);
          }

          break;
        case NIFUtil::ShapeShader::COMPLEXMATERIAL:
          MatchedPath = L"";
          ShouldApply = PatcherComplexMaterial::shouldApplySlots(SearchPrefixes, MatchedPath, EnableDynCubemaps, Key.NIFPath);

          if (ShouldApply) {
            NewSlots = PatcherComplexMaterial::applyPatchSlots(OldSlots, MatchedPath, EnableDynCubemaps);
            PatchTXST = true;
          } else {
            spdlog::warn("TXST record with Form ID {} is not able to be patched with Complex Material, but the mesh was. This will cause issues", TXSTFormID);
          }

          break;
        case NIFUtil::ShapeShader::TRUEPBR:
          map<size_t, tuple<nlohmann::json, wstring>> TruePBRData;
          ShouldApply = PatcherTruePBR::shouldApplySlots(L"Plugin Patching | ", SearchPrefixes, Key.NIFPath, TruePBRData);

          if (ShouldApply) {
            for (auto &TruePBRCFG : TruePBRData) {
              OldSlots = PatcherTruePBR::applyPatchSlots(OldSlots, get<0>(TruePBRCFG.second), get<1>(TruePBRCFG.second));
            }
            NewSlots = OldSlots;
            PatchTXST = true;
          } else {
            spdlog::warn("TXST record with Form ID {} is not able to be patched with TruePBR, but the mesh was. This will cause issues", TXSTFormID);
          }

          break;
      }

      if (!PatchTXST) {
        // No need to patch this one
        continue;
      }

      // Add required masters
      addRequiredMasters(TXSTId, PluginHandler, false);
      // Copy record
      auto NewTXST = copyElement(TXSTId, PluginHandler, false);

      // Loop through new slots
      for (int I = 0; I < NUM_TEXTURE_SLOTS - 1; I++) {
        // Remove Textures from the beggining of the slot name
        wstring PluginTexSlot = L"TX0" + to_wstring(I);
        auto NIFSlot = getNifSlotFromPluginSlot(PluginTexSlot);
        wstring TexSlotPath = NewSlots[static_cast<size_t>(NIFSlot)];
        if (boost::istarts_with(TexSlotPath, L"textures\\")) {
          TexSlotPath = TexSlotPath.substr(TEXTURES_LENGTH);
        }

        // TODO we should delete elements if they are empty instead of having the value blank

        // Add element if it doesn't exist
        wstring KeyPath = L"Textures (RGB/A)\\TX0" + to_wstring(I);
        addElementValue(NewTXST, KeyPath, TexSlotPath);
      }
    }
  }

  // Save file
  saveFile(PluginHandler, OutputDir / getOutputPluginName());

  spdlog::info("Done creating ParallaxGen.esp Plugin");

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto ParallaxGenPlugin::getPluginSlotFromNifSlot(const NIFUtil::TextureSlots &Slot) -> std::wstring {
  switch (Slot) {
    case NIFUtil::TextureSlots::DIFFUSE:
      return L"TX00";
    case NIFUtil::TextureSlots::NORMAL:
      return L"TX01";
    case NIFUtil::TextureSlots::GLOW:
      return L"TX03";
    case NIFUtil::TextureSlots::PARALLAX:
      return L"TX04";
    case NIFUtil::TextureSlots::CUBEMAP:
      return L"TX05";
    case NIFUtil::TextureSlots::ENVMASK:
      return L"TX02";
    case NIFUtil::TextureSlots::TINT:
      return L"TX06";
    case NIFUtil::TextureSlots::BACKLIGHT:
      return L"TX07";
    default:
      return L"TX00";
  }
}

auto ParallaxGenPlugin::getNifSlotFromPluginSlot(const std::wstring &Slot) -> NIFUtil::TextureSlots {
  if (Slot == L"TX00") {
    return NIFUtil::TextureSlots::DIFFUSE;
  }
  if (Slot == L"TX01") {
    return NIFUtil::TextureSlots::NORMAL;
  }
  if (Slot == L"TX03") {
    return NIFUtil::TextureSlots::GLOW;
  }
  if (Slot == L"TX04") {
    return NIFUtil::TextureSlots::PARALLAX;
  }
  if (Slot == L"TX05") {
    return NIFUtil::TextureSlots::CUBEMAP;
  }
  if (Slot == L"TX02") {
    return NIFUtil::TextureSlots::ENVMASK;
  }
  if (Slot == L"TX06") {
    return NIFUtil::TextureSlots::TINT;
  }
  if (Slot == L"TX07") {
    return NIFUtil::TextureSlots::BACKLIGHT;
  }

  return NIFUtil::TextureSlots::UNKNOWN;
}
