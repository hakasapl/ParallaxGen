#include "ParallaxGenPlugin.hpp"
#include "BethesdaGame.hpp"
#include "ParallaxGenTask.hpp"
#include "XEditLibCpp.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <spdlog/spdlog.h>
#include <windows.h>

using namespace std;

ParallaxGenPlugin::ParallaxGenPlugin(const BethesdaGame &Game)
{
  // Convert game type
  XEditLibCpp::GameMode Mode = XEditLibCpp::GameMode::SkyrimSE;
  switch (Game.getGameType())
  {
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
    return ParallaxGenTask::PGResult::FAILURE;
  }

  // Build references
  buildReferences(0, true);

  // Get TXST objects
  auto TXSTs = getRecords(0, L"TXST", true);

  // Find references to each TXST
  for (const auto &TXST : TXSTs) {
    auto TXSTFormID = getFormID(TXST, false);
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
        addAlternateTexturesToMap(getElement(Ref, L"MODL"), L"MODL", L"MODS");
      }

      if (hasElement(Ref, L"MOD2") && hasElement(Ref, L"MOD2\\MO2S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD2"), L"MOD2", L"MO2S");
      }

      if (hasElement(Ref, L"MOD3") && hasElement(Ref, L"MOD3\\MO3S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD3"), L"MOD3", L"MO3S");
      }

      if (hasElement(Ref, L"MOD4") && hasElement(Ref, L"MOD4\\MO4S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD4"), L"MOD4", L"MO4S");
      }

      if (hasElement(Ref, L"MOD5") && hasElement(Ref, L"MOD5\\MO5S")) {
        addAlternateTexturesToMap(getElement(Ref, L"MOD5"), L"MOD5", L"MO5S");
      }
    }
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

void ParallaxGenPlugin::addAlternateTexturesToMap(const unsigned int &Id, const std::wstring &MDLKey, const std::wstring &AltTXSTKey) {
  // Get the NIF name
  auto NIFName = getValue(Id, MDLKey);
  boost::to_lower(NIFName);  // Lowercase nif name

  // Loop through alternate textures
  auto AltTXSTs = getElements(Id, AltTXSTKey, false, false);
  for (const auto &AltTXST : AltTXSTs) {
    // Get 3D Name
    auto Name3D = getValue(AltTXST, L"3D Name");

    // Get New TXST
    auto NewTXSTRaw = getValue(AltTXST, L"New Texture");

    static boost::wregex TXSTRegex(L"\\[TXST:([0-9A-Fa-f]+)\\]");
    boost::wsmatch Matches;

    unsigned int TXSTFormID = 0;
    if (boost::regex_search(NewTXSTRaw, Matches, TXSTRegex)) {
      TXSTFormID = std::stoul(Matches[1], nullptr, FORMID_BASE);
    } else {
      return;
    }

    auto NewTXST = getRecord(0, TXSTFormID, true);

    // Get 3D Index
    auto Index3D = getIntValue(AltTXST, L"3D Index");

    // Create Key Val
    TXSTRefID KeyVal = {NIFName, Name3D, Index3D};
    TXSTRefsMap[KeyVal].insert(NewTXST);
  }
}
