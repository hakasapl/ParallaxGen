#include <CLI/CLI.hpp>

#include <NifFile.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/stacktrace/stacktrace.hpp>

#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include "BethesdaGame.hpp"
#include "GUI/LauncherWindow.hpp"
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGen.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenUtil.hpp"
#include "ParallaxGenWarnings.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherShader.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherUpgradeParallaxToCM.hpp"
#include "patchers/PatcherUtil.hpp"
#include "patchers/PatcherVanillaParallax.hpp"

#include "ParallaxGenUI.hpp"

constexpr unsigned MAX_LOG_SIZE = 5242880;
constexpr unsigned MAX_LOG_FILES = 1000;

using namespace std;

struct ParallaxGenCLIArgs {
  int Verbosity = 0;
  bool Autostart = false;
};

// Store game type strings and their corresponding BethesdaGame::GameType enum
// values This also determines the CLI argument help text (the key values)
auto getGameTypeMap() -> unordered_map<string, BethesdaGame::GameType> {
  static unordered_map<string, BethesdaGame::GameType> GameTypeMap = {
      {"skyrimse", BethesdaGame::GameType::SKYRIM_SE}, {"skyrimgog", BethesdaGame::GameType::SKYRIM_GOG},
      {"skyrim", BethesdaGame::GameType::SKYRIM},      {"skyrimvr", BethesdaGame::GameType::SKYRIM_VR},
      {"enderal", BethesdaGame::GameType::ENDERAL},    {"enderalse", BethesdaGame::GameType::ENDERAL_SE}};
  return GameTypeMap;
}

auto getGameTypeMapStr() -> string {
  const auto GameTypeMap = getGameTypeMap();
  static vector<string> GameTypeStrs;
  GameTypeStrs.reserve(GameTypeMap.size());
  for (const auto &Pair : GameTypeMap) {
    GameTypeStrs.push_back(Pair.first);
  }

  static string GameTypeStr = boost::join(GameTypeStrs, ", ");
  return GameTypeStr;
}

auto deployAssets(const filesystem::path &OutputDir, const filesystem::path &ExePath) -> void {
  // Install default cubemap file if needed
  static const filesystem::path DynCubeMapPath = "textures/cubemaps/dynamic1pxcubemap_black.dds";

  spdlog::info("Installing default dynamic cubemap file");

  // Create Directory
  const filesystem::path OutputCubemapPath = OutputDir / DynCubeMapPath.parent_path();
  filesystem::create_directories(OutputCubemapPath);

  boost::filesystem::path AssetPath = boost::filesystem::path(ExePath) / "assets/dynamic1pxcubemap_black_ENB.dds";
  boost::filesystem::path OutputPath = boost::filesystem::path(OutputDir) / DynCubeMapPath;

  // Move File
  boost::filesystem::copy_file(AssetPath, OutputPath, boost::filesystem::copy_options::overwrite_existing);

  spdlog::info("Installing neutral textures");

  const filesystem::path OutputAssetsPath = OutputDir / "textures\\parallaxgen";
  filesystem::create_directories(OutputAssetsPath);

  std::vector<filesystem::path> assets = {"neutral_m.dds", "neutral_p.dds", "neutral_rmaos.dds"};

  for (auto const &a : assets) {
    filesystem::path AssetPath = ExePath / "assets" / a;
    filesystem::path OutputPath = OutputAssetsPath / a;

    filesystem::copy_file(AssetPath, OutputPath, filesystem::copy_options::overwrite_existing);
  }
}

void mainRunner(ParallaxGenCLIArgs &Args, const filesystem::path &ExePath) {
  // Welcome Message
  spdlog::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);

  // Test message if required
  if (PARALLAXGEN_TEST_VERSION > 0) {
    spdlog::warn("This is an EXPERIMENTAL development build of ParallaxGen: {} Test Build {}", PARALLAXGEN_VERSION,
                 PARALLAXGEN_TEST_VERSION);
  }

  // Alpha message
  spdlog::warn("ParallaxGen is currently in ALPHA. Please file detailed bug reports on nexus or github.");

  // Create cfg directory if it does not exist
  const filesystem::path CfgDir = ExePath / "cfg";
  if (!filesystem::exists(CfgDir)) {
    filesystem::create_directories(CfgDir);
  }

  // Initialize ParallaxGenConfig
  auto PGC = ParallaxGenConfig(ExePath);
  PGC.loadConfig();

  // Initialize UI
  ParallaxGenUI::init();

  auto Params = PGC.getParams();

  // Show launcher UI
  if (!Args.Autostart) {
    Params = ParallaxGenUI::showLauncher(PGC);
  }

  // Validate config
  vector<string> Errors;
  if (!ParallaxGenConfig::validateParams(Params, Errors)) {
    for (const auto &Error : Errors) {
      spdlog::error("{}", Error);
    }
    spdlog::critical("Validation errors were found. Exiting.");
    exit(1);
  }

  // Print configuration parameters
  spdlog::debug(L"Configuration Parameters:\n\n{}\n", Params.getString());

  // print output location
  spdlog::info(L"ParallaxGen output directory: {}", Params.Output.Dir.wstring());

  // Create relevant objects
  auto BG = BethesdaGame(Params.Game.Type, true, Params.Game.Dir);

  auto MMD = ModManagerDirectory(Params.ModManager.Type);
  auto PGD = ParallaxGenDirectory(&BG, Params.Output.Dir, &MMD);
  auto PGD3D = ParallaxGenD3D(&PGD, Params.Output.Dir, ExePath, Params.Processing.GPUAcceleration);
  auto PG = ParallaxGen(Params.Output.Dir, &PGD, &PGD3D, Params.PostPatcher.OptimizeMeshes);

  Patcher::loadStatics(PGD, PGD3D);

  // Check if GPU needs to be initialized
  if (Params.Processing.GPUAcceleration) {
    PGD3D.initGPU();
  }

  //
  // Generation
  //

  // Get current time to compare later
  auto StartTime = chrono::high_resolution_clock::now();
  long long TimeTaken = 0;

  // Create output directory
  try {
    filesystem::create_directories(Params.Output.Dir);
  } catch (const filesystem::filesystem_error &E) {
    spdlog::error("Failed to create output directory: {}", E.what());
    exit(1);
  }

  // If output dir is the same as data dir meshes might get overwritten
  if (filesystem::equivalent(Params.Output.Dir, PGD.getDataPath())) {
    spdlog::critical("Output directory cannot be the same directory as your data folder. "
                     "Exiting.");
    exit(1);
  }

  // If output dir is a subdirectory of data dir vfs issues can occur
  if (boost::istarts_with(Params.Output.Dir.wstring(), BG.getGameDataPath().wstring() + "\\")) {
    spdlog::critical("Output directory cannot be a subdirectory of your data folder. Exiting.");
    exit(1);
  }

  // Check if dyndolod.esp exists
  const auto ActivePlugins = BG.getActivePlugins(false, true);
  if (find(ActivePlugins.begin(), ActivePlugins.end(), L"dyndolod.esp") != ActivePlugins.end()) {
    spdlog::critical("DynDoLOD and TexGen outputs must be disabled prior to running ParallaxGen. It is recommended to "
                     "generate LODs after running ParallaxGen with the ParallaxGen output enabled.");
    exit(1);
  }

  // Init PGP library
  if (Params.Processing.PluginPatching) {
    spdlog::info("Initializing plugin patcher");
    ParallaxGenPlugin::loadStatics(&PGD);
    ParallaxGenPlugin::initialize(BG);
    ParallaxGenPlugin::populateObjs();
  }

  // Populate file map from data directory
  if (Params.ModManager.Type == ModManagerDirectory::ModManagerType::ModOrganizer2 &&
      !Params.ModManager.MO2InstanceDir.empty() && !Params.ModManager.MO2Profile.empty()) {
    // MO2
    MMD.populateModFileMapMO2(Params.ModManager.MO2InstanceDir, Params.ModManager.MO2Profile, Params.Output.Dir);
  } else if (Params.ModManager.Type == ModManagerDirectory::ModManagerType::Vortex) {
    // Vortex
    MMD.populateModFileMapVortex(BG.getGameDataPath());
  }

  // delete existing output
  PG.deleteOutputDir();

  // Check if ParallaxGen output already exists in data directory
  const filesystem::path PGStateFilePath = BG.getGameDataPath() / ParallaxGen::getDiffJSONName();
  if (filesystem::exists(PGStateFilePath)) {
    spdlog::critical("ParallaxGen meshes exist in your data directory, please delete before "
                     "re-running.");
    exit(1);
  }

  // Init file map
  PGD.populateFileMap(Params.Processing.BSA);

  // Map files
  PGD.mapFiles(Params.MeshRules.BlockList, Params.MeshRules.AllowList, Params.TextureRules.TextureMaps,
               Params.TextureRules.VanillaBSAList, Params.Processing.MapFromMeshes, Params.Processing.Multithread,
               Params.Processing.HighMem);

  // Find CM maps
  spdlog::info("Finding complex material env maps");
  PGD3D.findCMMaps(Params.TextureRules.VanillaBSAList);
  spdlog::info("Done finding complex material env maps");

  // Create patcher factory
  PatcherUtil::PatcherSet Patchers;
  if (Params.ShaderPatcher.Parallax) {
    Patchers.ShaderPatchers.emplace(PatcherVanillaParallax::getShaderType(), PatcherVanillaParallax::getFactory());
  }
  if (Params.ShaderPatcher.ComplexMaterial) {
    Patchers.ShaderPatchers.emplace(PatcherComplexMaterial::getShaderType(), PatcherComplexMaterial::getFactory());
    PatcherComplexMaterial::loadStatics(Params.PrePatcher.DisableMLP,
                                        Params.ShaderPatcher.ShaderPatcherComplexMaterial.ListsDyncubemapBlocklist);
  }
  if (Params.ShaderPatcher.TruePBR) {
    Patchers.ShaderPatchers.emplace(PatcherTruePBR::getShaderType(), PatcherTruePBR::getFactory());
    PatcherTruePBR::loadStatics(PGD.getPBRJSONs());
  }
  if (Params.ShaderTransforms.ParallaxToCM) {
    Patchers.ShaderTransformPatchers[PatcherUpgradeParallaxToCM::getFromShader()].emplace(
        PatcherUpgradeParallaxToCM::getToShader(), PatcherUpgradeParallaxToCM::getFactory());
  }

  if (Params.ModManager.Type != ModManagerDirectory::ModManagerType::None) {
    // Check if MO2 is used and MO2 use order is checked
    if (Params.ModManager.Type == ModManagerDirectory::ModManagerType::ModOrganizer2 && Params.ModManager.MO2UseOrder) {
      // Get mod order from MO2
      const auto &ModOrder = MMD.getInferredOrder();
      PGC.setModOrder(ModOrder);
    } else {
      // Find conflicts
      const auto ModConflicts =
          PG.findModConflicts(Patchers, Params.Processing.Multithread, Params.Processing.PluginPatching);
      const auto ExistingOrder = PGC.getModOrder();

      if (!ModConflicts.empty()) {
        // pause timer for UI
        TimeTaken += chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - StartTime).count();

        // Select mod order
        auto SelectedOrder = ParallaxGenUI::selectModOrder(ModConflicts, ExistingOrder);
        StartTime = chrono::high_resolution_clock::now();

        PGC.setModOrder(SelectedOrder);
      }
    }
  }

  // Patch meshes if set
  auto ModPriorityMap = PGC.getModPriorityMap();
  ParallaxGenWarnings::init(&PGD, &ModPriorityMap);
  if (Params.ShaderPatcher.Parallax || Params.ShaderPatcher.ComplexMaterial || Params.ShaderPatcher.TruePBR) {
    PG.patchMeshes(Patchers, &ModPriorityMap, Params.Processing.Multithread, Params.Processing.PluginPatching);
  }

  // Release cached files, if any
  PGD.clearCache();

  spdlog::info("ParallaxGen has finished patching meshes.");

  // Write plugin
  if (Params.Processing.PluginPatching) {
    spdlog::info("Saving ParallaxGen.esp");
    ParallaxGenPlugin::savePlugin(Params.Output.Dir, Params.Processing.PluginESMify);
  }

  deployAssets(Params.Output.Dir, ExePath);

  // archive
  if (Params.Output.Zip) {
    PG.zipMeshes();
    PG.deleteOutputDir(false);
  }

  const auto EndTime = chrono::high_resolution_clock::now();
  TimeTaken += chrono::duration_cast<chrono::seconds>(EndTime - StartTime).count();

  spdlog::info("ParallaxGen took {} seconds to complete", TimeTaken);
}

void exitBlocking() {
  if (LauncherWindow::UIExitTriggered) {
    return;
  }

  cout << "Press ENTER to exit...";
  cin.get();
}

auto getExecutablePath() -> filesystem::path {
  wchar_t Buffer[MAX_PATH];                                 // NOLINT
  if (GetModuleFileNameW(nullptr, Buffer, MAX_PATH) == 0) { // NOLINT
    cerr << "Error getting executable path: " << GetLastError() << "\n";
    exit(1);
  }

  filesystem::path OutPath = filesystem::path(Buffer);

  if (filesystem::exists(OutPath)) {
    return OutPath;
  }

  cerr << "Error getting executable path: path does not exist\n";
  exit(1);

  return {};
}

void addArguments(CLI::App &App, ParallaxGenCLIArgs &Args) {
  // Logging
  App.add_flag("-v", Args.Verbosity,
               "Verbosity level -v for DEBUG data or -vv for TRACE data "
               "(warning: TRACE data is very verbose)");
  App.add_flag("--autostart", Args.Autostart, "Start generation without user input");
}

void initLogger(const filesystem::path &LOGPATH, const ParallaxGenCLIArgs &Args) {
  // Create loggers
  vector<spdlog::sink_ptr> Sinks;
  auto ConsoleSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
  Sinks.push_back(ConsoleSink);

  // Rotating file sink
  auto FileSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(LOGPATH.wstring(), MAX_LOG_SIZE, MAX_LOG_FILES);
  Sinks.push_back(FileSink);
  auto Logger = make_shared<spdlog::logger>("ParallaxGen", Sinks.begin(), Sinks.end());

  // register logger parameters
  spdlog::register_logger(Logger);
  spdlog::set_default_logger(Logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  // Set logging mode
  if (Args.Verbosity >= 1) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
    spdlog::debug("DEBUG logging enabled");
  }

  if (Args.Verbosity >= 2) {
    spdlog::set_level(spdlog::level::trace);
    ConsoleSink->set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::trace);
    spdlog::trace("TRACE logging enabled");
  }
}

auto main(int ArgC, char **ArgV) -> int {
// Block until enter only in debug mode
#ifdef _DEBUG
  cout << "Press ENTER to start (DEBUG mode)...";
  cin.get();
#endif

  // This is what keeps the console window open after the program exits until user input
  if (atexit(exitBlocking) != 0) {
    cerr << "Failed to register exitBlocking function\n";
    return 1;
  }

  SetConsoleOutputCP(CP_UTF8);

  // Find location of ParallaxGen.exe
  const filesystem::path ExePath = getExecutablePath().parent_path();

  // CLI Arguments
  ParallaxGenCLIArgs Args;
  CLI::App App{"ParallaxGen: Auto convert meshes to parallax meshes"};
  addArguments(App, Args);

  // Parse CLI Arguments (this is what exits on any validation issues)
  CLI11_PARSE(App, ArgC, ArgV);

  // Initialize logger
  const filesystem::path LogDir = ExePath / "log";
  // delete old logs
  if (filesystem::exists(LogDir)) {
    try {
      // Only delete files that are .log and start with ParallaxGen
      for (const auto &Entry : filesystem::directory_iterator(LogDir)) {
        if (Entry.is_regular_file() && Entry.path().extension() == ".log" &&
            Entry.path().filename().wstring().starts_with(L"ParallaxGen")) {
          filesystem::remove(Entry.path());
        }
      }
    } catch (const filesystem::filesystem_error &E) {
      cerr << "Failed to delete old logs: " << E.what() << "\n";
      return 1;
    }
  }

  const filesystem::path LogPath = LogDir / "ParallaxGen.log";
  initLogger(LogPath, Args);

  // Main Runner (Catches all exceptions)
  try {
    mainRunner(Args, ExePath);
  } catch (const exception &E) {
    auto Trace = boost::stacktrace::stacktrace();
    spdlog::critical("An unhandled exception occurred (Please provide this entire message "
                     "in your bug report).\n\nException type: {}\nMessage: {}\nStack Trace: "
                     "\n{}",
                     typeid(E).name(), E.what(), boost::stacktrace::to_string(Trace));
    cout << "Press ENTER to abort...";
    cin.get();
    abort();
  }
}
