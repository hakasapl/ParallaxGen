#include "ParallaxGenPlugin.hpp"
#include "BethesdaGame.hpp"
#include "XEditLibCpp.hpp"

#include <boost/algorithm/string.hpp>
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

void ParallaxGenPlugin::init() {
  // Load Plugins
  loadPlugins(ActivePlugins, false);

  auto LoaderStatus = getLoaderStatus();
  while (LoaderStatus != XEditLibCpp::LoaderStates::IsDone && LoaderStatus != XEditLibCpp::LoaderStates::IsError) {
    LoaderStatus = getLoaderStatus();
    Sleep(PLUGIN_LOADER_CHECK_INTERVAL);
  }

  spdlog::info("Plugin Loader is done");
}
