#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <windows.h>

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/stacktrace.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include "BethesdaGame/BethesdaGame.hpp"
#include "ParallaxGen/ParallaxGen.hpp"
#include "ParallaxGenD3D/ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"
#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

auto getExecutablePath() -> filesystem::path {
  char Buffer[MAX_PATH];                                   // NOLINT
  if (GetModuleFileName(nullptr, Buffer, MAX_PATH) == 0) { // NOLINT
    cerr << "Error getting executable path: " << GetLastError() << "\n";
    exitWithUserInput(1);
  }

  filesystem::path OutPath = filesystem::path(Buffer);

  if (filesystem::exists(OutPath)) {
    return OutPath;
  }

  cerr << "Error getting executable path: path does not exist\n";
  exitWithUserInput(1);

  return {};
}

void initLogger(const filesystem::path &LOGPATH) {
  // Create loggers
  vector<spdlog::sink_ptr> Sinks;
  Sinks.push_back(make_shared<spdlog::sinks::stdout_color_sink_mt>());
  Sinks.push_back(make_shared<spdlog::sinks::basic_file_sink_mt>(
      LOGPATH.string(), true)); // TODO wide string support here
  auto Logger =
      make_shared<spdlog::logger>("ParallaxGen", Sinks.begin(), Sinks.end());

  // register logger parameters
  spdlog::register_logger(Logger);
  spdlog::set_default_logger(Logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);
}

void addArguments(CLI::App &App, int &Verbosity, filesystem::path &GameDir,
                  string &GameType, filesystem::path &OutputDir,
                  bool &Autostart, bool &UpgradeShaders, bool &OptimizeMeshes,
                  bool &NoZip, bool &NoCleanup, bool &NoDefaultConfig,
                  bool &IgnoreParallax, bool &IgnoreComplexMaterial,
                  bool &IgnoreTruePBR, const string &AvailableGameTypesStr) {
  App.add_flag("-v", Verbosity,
               "Verbosity level -v for DEBUG data or -vv for TRACE data "
               "(warning: TRACE data is very verbose)");
  App.add_option("-d,--game-dir", GameDir, "Manually specify game directory");
  App.add_option("-g,--game-type", GameType,
                 "Specify game type [" + AvailableGameTypesStr + "]");
  App.add_option("-o,--output-dir", OutputDir,
                 "Manually specify output directory");
  App.add_flag("--Autostart", Autostart, "Start generation without user input");
  App.add_flag("--upgrade-shaders", UpgradeShaders,
               "Upgrade shaders to a better version whenever possible");
  App.add_flag("--optimize-meshes", OptimizeMeshes,
               "Optimize meshes before saving them");
  App.add_flag("--no-zip", NoZip,
               "Don't zip the output meshes (also enables --no-cleanup)");
  App.add_flag("--no-cleanup", NoCleanup,
               "Don't delete generated meshes after zipping");
  App.add_flag("--no-default-conifg", NoDefaultConfig,
               "Don't load the default config file (You need to know what "
               "you're doing for this)");
  App.add_flag("--ignore-parallax", IgnoreParallax,
               "Don't generate any parallax meshes");
  App.add_flag("--ignore-complex-material", IgnoreComplexMaterial,
               "Don't generate any complex material meshes");
  App.add_flag("--ignore-truepbr", IgnoreTruePBR,
               "Don't apply any TruePBR configs in the load order");
}

// Store game type strings and their corresponding BethesdaGame::GameType enum
// values This also determines the CLI argument help text (the key values)
auto getGameTypeMap() -> unordered_map<string, BethesdaGame::GameType> {
  static unordered_map<string, BethesdaGame::GameType> GameTypeMap = {
      {"skyrimse", BethesdaGame::GameType::SKYRIM_SE},
      {"skyrimgog", BethesdaGame::GameType::SKYRIM_GOG},
      {"skyrim", BethesdaGame::GameType::SKYRIM},
      {"skyrimvr", BethesdaGame::GameType::SKYRIM_VR},
      {"enderal", BethesdaGame::GameType::ENDERAL},
      {"enderalse", BethesdaGame::GameType::ENDERAL_SE}};
  return GameTypeMap;
}

void mainRunner(int ArgC, char **ArgV) {
  // Find location of ParallaxGen.exe
  const filesystem::path ExePath = getExecutablePath().parent_path();

  // Initialize logger
  const filesystem::path LogPath = ExePath / "ParallaxGen.log";
  initLogger(LogPath);

  //
  // CLI Arguments
  //
  const string StartupCLIArgs =
      boost::join(vector<string>(ArgV + 1, ArgV + ArgC), " "); // NOLINT

  // Define game type vars
  vector<string> AvailableGameTypes;
  auto GameTypeMap = getGameTypeMap();
  AvailableGameTypes.reserve(GameTypeMap.size());
  for (const auto &Pair : GameTypeMap) {
    AvailableGameTypes.push_back(Pair.first);
  }
  const string AvailableGameTypesStr = boost::join(AvailableGameTypes, ", ");

  // Vars that store CLI argument values
  // Default values are defined here
  // TODO we should probably define a structure to hold CLI arguments
  int Verbosity = 0;
  filesystem::path GameDir;
  string GameType = "skyrimse";
  filesystem::path OutputDir = ExePath / "ParallaxGen_Output";
  bool Autostart = false;
  bool UpgradeShaders = false;
  bool OptimizeMeshes = false;
  bool NoZip = false;
  bool NoCleanup = false;
  bool NoDefaultConfig = false;
  bool IgnoreParallax = false;
  bool IgnoreComplexMaterial = false;
  bool IgnoreTruepbr = false;

  // Parse CLI arguments
  CLI::App App{"ParallaxGen: Auto convert meshes to parallax meshes"};
  addArguments(App, Verbosity, GameDir, GameType, OutputDir, Autostart,
               UpgradeShaders, OptimizeMeshes, NoZip, NoCleanup,
               NoDefaultConfig, IgnoreParallax, IgnoreComplexMaterial,
               IgnoreTruepbr, AvailableGameTypesStr);

  // Check if arguments are valid, otherwise print error to user
  try {
    App.parse(ArgC, ArgV);
  } catch (const CLI::ParseError &E) {
    // TODO --help doesn't work correctly with this
    spdlog::critical("Arguments Invalid: {}\n{}", E.what(), App.help());
    exitWithUserInput(1);
  }

  //
  // Welcome Message
  //
  spdlog::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);
  spdlog::info("Arguments Supplied: {}", StartupCLIArgs);

  //
  // Argument validation
  //

  // Check if game_type exists in allowed_game_types
  if (GameTypeMap.find(GameType) == GameTypeMap.end()) {
    spdlog::critical("Invalid game type (-g) specified: {}", GameType);
    exitWithUserInput(1);
  }

  // Check if there is actually anything to do
  if (IgnoreParallax && IgnoreComplexMaterial && IgnoreTruepbr &&
      !UpgradeShaders) {
    spdlog::critical(
        "There is nothing to do if both --ignore-parallax and "
        "--ignore-complex-material are set and --upgrade-shaders is not set.");
    exitWithUserInput(1);
  }

  // If --no-zip is set, also set --no-cleanup
  if (NoZip) {
    NoCleanup = true;
  }

  // Set logging mode
  if (Verbosity == 1) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
    spdlog::debug("DEBUG logging enabled");
  }

  if (Verbosity >= 2) {
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
    spdlog::trace("TRACE logging enabled");
  }

  // If true, NIF patching occurs
  bool PatchMeshes =
      !IgnoreParallax || !IgnoreComplexMaterial || !IgnoreTruepbr;

  //
  // Init
  //

  // print output location
  spdlog::info(
      L"ParallaxGen output directory (the contents will be deleted if you "
      L"start generation!): {}",
      OutputDir.wstring());

  // Create bethesda game type object
  BethesdaGame::GameType BGType = GameTypeMap.at(GameType);

  // Create relevant objects
  BethesdaGame BG = BethesdaGame(BGType, GameDir, true);
  ParallaxGenDirectory PGD = ParallaxGenDirectory(BG, ExePath);
  ParallaxGenD3D PGD3D = ParallaxGenD3D(&PGD, OutputDir, ExePath);
  ParallaxGen PG =
      ParallaxGen(OutputDir, &PGD, &PGD3D, OptimizeMeshes, IgnoreParallax,
                  IgnoreComplexMaterial, IgnoreTruepbr);

  // Check if GPU needs to be initialized
  if (UpgradeShaders) {
    PGD3D.initGPU();
  }

  // Create output directory
  try {
    filesystem::create_directories(OutputDir);
  } catch (const filesystem::filesystem_error &E) {
    spdlog::error("Failed to create output directory: {}", E.what());
    exitWithUserInput(1);
  }

  // If output dir is the same as data dir meshes might get overwritten
  if (filesystem::equivalent(OutputDir, PGD.getDataPath())) {
    spdlog::critical(
        "Output directory cannot be the same directory as your data folder. "
        "Exiting.");
    exitWithUserInput(1);
  }

  //
  // Generation
  //

  // User Input to Continue
  if (!Autostart) {
    cout << "Press ENTER to start ParallaxGen...";
    cin.get();
  }

  // delete existing output
  PG.deleteOutputDir();

  // Check if ParallaxGen output already exists in data directory
  const filesystem::path PGStateFilePath =
      BG.getGameDataPath() / ParallaxGen::parallax_state_file;
  if (filesystem::exists(PGStateFilePath)) {
    spdlog::critical(
        "ParallaxGen meshes exist in your data directory, please delete before "
        "re-running.");
    exitWithUserInput(1);
  }

  PG.initOutputDir();

  // Populate file map from data directory
  PGD.populateFileMap();

  // Load configs
  PGD.loadPGConfig(!NoDefaultConfig);

  // Install default cubemap file if needed
  if (!IgnoreComplexMaterial) {
    // install default cubemap file if needed
    if (!PGD.defCubemapExists()) {
      spdlog::info("Installing default cubemap file");

      // Create Directory
      filesystem::path OutputCubemapPath =
          OutputDir / ParallaxGenDirectory::default_cubemap_path.parent_path();
      filesystem::create_directories(OutputCubemapPath);

      boost::filesystem::path AssetPath =
          boost::filesystem::path(ExePath) /
          L"assets/dynamic1pxcubemap_black_ENB.dds";
      boost::filesystem::path OutputPath =
          boost::filesystem::path(OutputDir) /
          ParallaxGenDirectory::default_cubemap_path;

      // Move File
      boost::filesystem::copy_file(
          AssetPath, OutputPath,
          boost::filesystem::copy_options::overwrite_existing);
    }
  }

  // Build file vectors
  if (!IgnoreParallax || (IgnoreParallax && UpgradeShaders)) {
    PGD.findHeightMaps();
  }

  if (!IgnoreComplexMaterial) {
    PGD.findComplexMaterialMaps();
  }

  if (!IgnoreTruepbr) {
    PGD.findTruePBRConfigs();
  }

  // Upgrade shaders if set
  if (UpgradeShaders) {
    PG.upgradeShaders();
  }

  // Patch meshes if set
  if (PatchMeshes) {
    PGD.findMeshes();
    PG.patchMeshes();
  }

  spdlog::info("ParallaxGen has finished generating meshes.");

  // archive
  if (!NoZip) {
    PG.zipMeshes();
  }

  // cleanup
  if (!NoCleanup) {
    PG.deleteMeshes();
  }
}

int main(int ArgC, char **ArgV) {
  try {
    mainRunner(ArgC, ArgV);
    exitWithUserInput(0);
  } catch (const exception &E) {
    spdlog::critical(
        "An unhandled exception occurred (Please provide this entire message "
        "in your bug report).\n\nException type: {}\nMessage: {}\nStack Trace: "
        "\n{}",
        typeid(E).name(), E.what(),
        boost::stacktrace::to_string(boost::stacktrace::stacktrace()));
    exitWithUserInput(1);
  }
}
