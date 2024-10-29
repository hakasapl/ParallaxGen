#include <CLI/CLI.hpp>

#include <NifFile.hpp>
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
#include "ModManagerDirectory.hpp"
#include "ParallaxGen.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenUI.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherShader.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherVanillaParallax.hpp"


constexpr unsigned MAX_LOG_SIZE = 5242880;
constexpr unsigned MAX_LOG_FILES = 100;

using namespace std;

struct ParallaxGenCLIArgs {
  int Verbosity = 0;
  filesystem::path GameDir;
  string GameType = "skyrimse";
  filesystem::path OutputDir;
  string ModManagerType = "none";
  filesystem::path MO2InstanceDir;
  string MO2Profile = "Default";
  bool Autostart = false;
  bool NoMultithread = false;
  bool HighMem = false;
  bool NoGPU = false;
  bool NoBSA = false;
  bool UpgradeShaders = false;
  bool OptimizeMeshes = false;
  bool NoMapFromMeshes = false;
  bool NoPlugin = false;
  bool NoZip = false;
  bool NoCleanup = false;
  bool NoDefaultConfig = false;
  bool IgnoreParallax = false;
  bool IgnoreComplexMaterial = false;
  bool IgnoreTruePBR = false;
  bool DisableMLP = false;

  [[nodiscard]] auto getString() const -> string {
    string OutStr;
    OutStr += "Verbosity: " + to_string(Verbosity) + "\n";
    OutStr += "GameDir: " + GameDir.string() + "\n";
    OutStr += "GameType: " + GameType + "\n";
    OutStr += "OutputDir: " + OutputDir.string() + "\n";
    OutStr += "ModManagerType: " + ModManagerType + "\n";
    OutStr += "MO2InstanceDir: " + MO2InstanceDir.string() + "\n";
    OutStr += "MO2ProfileDir: " + MO2Profile + "\n";
    OutStr += "Autostart: " + to_string(static_cast<int>(Autostart)) + "\n";
    OutStr += "NoMultithread: " + to_string(static_cast<int>(NoMultithread)) + "\n";
    OutStr += "HighMem: " + to_string(static_cast<int>(HighMem)) + "\n";
    OutStr += "NoGPU: " + to_string(static_cast<int>(NoGPU)) + "\n";
    OutStr += "NoBSA: " + to_string(static_cast<int>(NoBSA)) + "\n";
    OutStr += "UpgradeShaders: " + to_string(static_cast<int>(UpgradeShaders)) + "\n";
    OutStr += "OptimizeMeshes: " + to_string(static_cast<int>(OptimizeMeshes)) + "\n";
    OutStr += "NoMapFromMeshes: " + to_string(static_cast<int>(NoMapFromMeshes)) + "\n";
    OutStr += "NoPlugin: " + to_string(static_cast<int>(NoPlugin)) + "\n";
    OutStr += "NoZip: " + to_string(static_cast<int>(NoZip)) + "\n";
    OutStr += "NoCleanup: " + to_string(static_cast<int>(NoCleanup)) + "\n";
    OutStr += "NoDefaultConfig: " + to_string(static_cast<int>(NoDefaultConfig)) + "\n";
    OutStr += "IgnoreParallax: " + to_string(static_cast<int>(IgnoreParallax)) + "\n";
    OutStr += "IgnoreComplexMaterial: " + to_string(static_cast<int>(IgnoreComplexMaterial)) + "\n";
    OutStr += "IgnoreTruePBR: " + to_string(static_cast<int>(IgnoreTruePBR)) + "\n";
    OutStr += "DisableMLP: " + to_string(static_cast<int>(DisableMLP));

    return OutStr;
  }
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

auto deployDynamicCubemapFile(ParallaxGenDirectory *PGD, const filesystem::path &OutputDir,
                              const filesystem::path &ExePath) -> void {
  // Install default cubemap file if needed
  static const filesystem::path DynCubeMapPath = "textures/cubemaps/dynamic1pxcubemap_black.dds";
  if (!PGD->isFile(DynCubeMapPath)) {
    spdlog::info("Installing default dynamic cubemap file");

    // Create Directory
    const filesystem::path OutputCubemapPath = OutputDir / DynCubeMapPath.parent_path();
    filesystem::create_directories(OutputCubemapPath);

    boost::filesystem::path AssetPath = boost::filesystem::path(ExePath) / "assets/dynamic1pxcubemap_black_ENB.dds";
    boost::filesystem::path OutputPath = boost::filesystem::path(OutputDir) / DynCubeMapPath;

    // Move File
    boost::filesystem::copy_file(AssetPath, OutputPath, boost::filesystem::copy_options::overwrite_existing);
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

  // Print configuration parameters
  spdlog::debug("Configuration Parameters:\n\n{}\n", Args.getString());

  // print output location
  spdlog::info(L"ParallaxGen output directory (the contents will be deleted if you "
               L"start generation!): {}",
               Args.OutputDir.wstring());

  // Create bethesda game type object
  BethesdaGame::GameType BGType = getGameTypeMap().at(Args.GameType); // NOLINT

  // Create relevant objects
  const auto BG = BethesdaGame(BGType, true, Args.GameDir);

  ModManagerDirectory::ModManagerType MMType = ModManagerDirectory::ModManagerType::None;
  if (Args.ModManagerType == "vortex") {
    MMType = ModManagerDirectory::ModManagerType::Vortex;
  } else if (Args.ModManagerType == "mo2") {
    MMType = ModManagerDirectory::ModManagerType::ModOrganizer2;
  }

  // Create patcher factory
  vector<function<unique_ptr<PatcherShader>(filesystem::path, nifly::NifFile *)>> PatcherFactories;
  if (!Args.IgnoreParallax) {
    PatcherFactories.emplace_back([](const filesystem::path &NIFFile, nifly::NifFile *NIF) {
      return make_unique<PatcherVanillaParallax>(NIFFile, NIF);
    });
  }
  if (!Args.IgnoreComplexMaterial) {
    PatcherFactories.emplace_back([](const filesystem::path &NIFFile, nifly::NifFile *NIF) {
      return make_unique<PatcherComplexMaterial>(NIFFile, NIF);
    });
  }
  if (!Args.IgnoreTruePBR) {
    PatcherFactories.emplace_back(
        [](const filesystem::path &NIFFile, nifly::NifFile *NIF) { return make_unique<PatcherTruePBR>(NIFFile, NIF); });
  }

  auto MMD = ModManagerDirectory(MMType);
  auto PGD = ParallaxGenDirectory(BG, Args.OutputDir, &MMD);
  auto PGC = ParallaxGenConfig(ExePath);
  auto PGD3D = ParallaxGenD3D(&PGD, Args.OutputDir, ExePath, !Args.NoGPU);
  auto PG = ParallaxGen(Args.OutputDir, &PGD, &PGC, &PGD3D, Args.OptimizeMeshes, Args.UpgradeShaders);

  // Initialize UI
  ParallaxGenUI::init();

  // Check if GPU needs to be initialized
  if (!Args.NoGPU) {
    PGD3D.initGPU();
  }

  //
  // Generation
  //

  // User Input to Continue
  if (!Args.Autostart) {
    cout << "Press ENTER to start ParallaxGen...";
    cin.get();
  }

  // Get current time to compare later
  auto StartTime = chrono::high_resolution_clock::now();
  long long TimeTaken = 0;

  // Create output directory
  try {
    filesystem::create_directories(Args.OutputDir);
  } catch (const filesystem::filesystem_error &E) {
    spdlog::error("Failed to create output directory: {}", E.what());
    exit(1);
  }

  // If output dir is the same as data dir meshes might get overwritten
  if (filesystem::equivalent(Args.OutputDir, PGD.getDataPath())) {
    spdlog::critical("Output directory cannot be the same directory as your data folder. "
                     "Exiting.");
    exit(1);
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

  // Init PGP library
  if (!Args.NoPlugin) {
    spdlog::info("Initializing plugin patcher");
    ParallaxGenPlugin::loadStatics(&PGD, &PGC);
    ParallaxGenPlugin::initialize(BG);
    ParallaxGenPlugin::populateObjs();
  }

  // Load configs
  PGC.loadConfig(!Args.NoDefaultConfig);

  // Populate file map from data directory
  if (MMType == ModManagerDirectory::ModManagerType::ModOrganizer2 && !Args.MO2InstanceDir.empty() &&
      !Args.MO2Profile.empty()) {
    // MO2
    auto MO2StagingFolder = Args.MO2InstanceDir / "mods";
    auto MO2ModlistTXT = Args.MO2InstanceDir / "profiles" / Args.MO2Profile / "modlist.txt";
    MMD.populateInfo(MO2ModlistTXT, MO2StagingFolder);
    MMD.populateModFileMap();
  } else if (MMType == ModManagerDirectory::ModManagerType::Vortex) {
    // Vortex
    MMD.populateInfo(BG.getGameDataPath() / "vortex.deployment.json");
    MMD.populateModFileMap();
  }

  PGD.populateFileMap(!Args.NoBSA);

  auto VanillaBSAList = PGC.getVanillaBSAList();

  // Map files
  PGD.mapFiles(PGC.getNIFBlocklist(), PGC.getManualTextureMaps(), VanillaBSAList, !Args.NoMapFromMeshes,
               !Args.NoMultithread, Args.HighMem);

  // Find CM maps
  spdlog::info("Finding complex material env maps");
  PGD3D.findCMMaps(VanillaBSAList);
  spdlog::info("Done finding complex material env maps");

  // Load patcher static vars
  PatcherShader::loadStatics(PGD, PGD3D);
  PatcherTruePBR::loadStatics(PGD.getPBRJSONs());
  PatcherComplexMaterial::loadStatics(Args.DisableMLP, PGC.getDynCubemapBlocklist());
  // PatcherVanillaParallax::loadStatics();

  if (MMType != ModManagerDirectory::ModManagerType::None) {
    // Find conflicts
    const auto ModConflicts = PG.findModConflicts(PatcherFactories, !Args.NoMultithread, !Args.NoPlugin);
    const auto ExistingOrder = PGC.getModOrder();

    // pause timer for UI
    TimeTaken += chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - StartTime).count();

    // Select mod order
    auto SelectedOrder = ParallaxGenUI::selectModOrder(ModConflicts, ExistingOrder);
    StartTime = chrono::high_resolution_clock::now();

    PGC.setModOrder(SelectedOrder);
  }

  // Patch meshes if set
  if (!Args.IgnoreParallax || !Args.IgnoreComplexMaterial || !Args.IgnoreTruePBR) {
    PG.patchMeshes(PatcherFactories, !Args.NoMultithread, !Args.NoPlugin);
  }

  // Release cached files, if any
  PGD.clearCache();

  spdlog::info("ParallaxGen has finished patching meshes.");

  // Write plugin
  if (!Args.NoPlugin) {
    spdlog::info("Saving ParallaxGen.esp");
    ParallaxGenPlugin::savePlugin(Args.OutputDir);
  }

  // Deploy dynamic cubemap file
  deployDynamicCubemapFile(&PGD, Args.OutputDir, ExePath);

  // archive
  if (!Args.NoZip) {
    PG.zipMeshes();
  }

  // cleanup
  if (!Args.NoCleanup) {
    PG.deleteMeshes();
  }

  const auto EndTime = chrono::high_resolution_clock::now();
  TimeTaken += chrono::duration_cast<chrono::seconds>(EndTime - StartTime).count();

  spdlog::info("ParallaxGen took {} seconds to complete", TimeTaken);
}

void exitBlocking() {
  cout << "Press ENTER to exit...";
  cin.get();
}

auto getExecutablePath() -> filesystem::path {
  char Buffer[MAX_PATH];                                   // NOLINT
  if (GetModuleFileName(nullptr, Buffer, MAX_PATH) == 0) { // NOLINT
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

void addArguments(CLI::App &App, ParallaxGenCLIArgs &Args, const filesystem::path &ExePath) {
  // Logging
  App.add_flag("-v", Args.Verbosity,
               "Verbosity level -v for DEBUG data or -vv for TRACE data "
               "(warning: TRACE data is very verbose)");
  // Game
  App.add_option("-d,--game-dir", Args.GameDir, "Manually specify game directory");
  App.add_option("-g,--game-type", Args.GameType, "Specify game type [" + getGameTypeMapStr() + "]");
  App.add_option("-m,--mod-manager", Args.ModManagerType, R"(Specify mod manager type ("mo2" or "vortex"))");
  App.add_option("--mo2-instance-dir", Args.MO2InstanceDir, "MO2 ONLY: Specify MO2 instance directory");
  App.add_option("--mo2-profile", Args.MO2Profile, "MO2 ONLY: Specify MO2 profile to use if other than 'Default'");
  App.add_flag("--no-bsa", Args.NoBSA, "Don't load BSA files, only loose files");
  // App Options
  App.add_flag("--autostart", Args.Autostart, "Start generation without user input");
  App.add_flag("--no-multithread", Args.NoMultithread, "Don't use multithreading (Slower)");
  auto *FlagNoGpu = App.add_flag("--no-gpu", Args.NoGPU, "Don't use the GPU for any operations (Slower)");
  App.add_flag("--no-default-conifg", Args.NoDefaultConfig,
               "Don't load the default config file (You need to know what "
               "you're doing for this)");
  // Output
  App.add_option("-o,--output-dir", Args.OutputDir, "Manually specify output directory");
  App.add_flag("--optimize-meshes", Args.OptimizeMeshes, "Optimize meshes before saving them");
  App.add_flag("--no-map-from-meshes", Args.NoMapFromMeshes,
               "Don't map textures from meshes (faster but less accurate)");
  App.add_flag("--no-plugin", Args.NoPlugin, "Don't create a ParallaxGen.esp plugin");
  App.add_flag("--high-mem", Args.HighMem, "Enable high memory usage (faster runtime but uses a lot more RAM)");
  App.add_flag("--no-zip", Args.NoZip, "Don't zip the output meshes (also enables --no-cleanup)");
  App.add_flag("--no-cleanup", Args.NoCleanup, "Don't delete generated meshes after zipping");
  // Patchers
  auto *FlagIgnoreParallax =
      App.add_flag("--ignore-parallax", Args.IgnoreParallax, "Don't generate any parallax meshes");
  auto *FlagIgnoreCM = App.add_flag("--ignore-complex-material", Args.IgnoreComplexMaterial,
                                    "Don't generate any complex material meshes");
  App.add_flag("--ignore-truepbr", Args.IgnoreTruePBR, "Don't apply any TruePBR configs in the load order");
  App.add_flag("--disable-mlp", Args.DisableMLP, "Disable MLP (Multi-Layer Parallax) if complex material is possible")
      ->excludes(FlagIgnoreCM);
  App.add_flag("--upgrade-shaders", Args.UpgradeShaders, "Upgrade shaders to a better version whenever possible")
      ->excludes(FlagNoGpu)
      ->excludes(FlagIgnoreParallax)
      ->excludes(FlagIgnoreCM);

  // Multi-argument Validation
  App.callback([&Args, &ExePath]() {
    // One action needs to be enabled
    if (Args.IgnoreParallax && Args.IgnoreComplexMaterial && Args.IgnoreTruePBR && !Args.UpgradeShaders) {
      throw CLI::ValidationError("No action items to do (check that you are not ignoring all patchers)");
    }

    // Validate Game Type
    const auto GameTypeMap = getGameTypeMap();
    if (GameTypeMap.find(Args.GameType) == GameTypeMap.end()) {
      throw CLI::ValidationError("Invalid game type (-g) specified: " + Args.GameType + ". Available options are [" +
                                 getGameTypeMapStr() + "]");
    }

    // Check if output dir is set, otherwise set default
    if (Args.OutputDir.empty()) {
      Args.OutputDir = ExePath / "ParallaxGen_Output";
    } else {
      // Check if output dir is a directory
      if (!filesystem::is_directory(Args.OutputDir) && filesystem::exists(Args.OutputDir)) {
        throw CLI::ValidationError("Output directory (-o) must be a directory or not exist");
      }
    }

    // If --no-zip is set, also set --no-cleanup
    if (Args.NoZip) {
      Args.NoCleanup = true;
    }

    if (Args.ModManagerType == "mo2" && Args.MO2InstanceDir.empty()) {
      throw CLI::ValidationError("MO2 instance directory must be specified with --mo2-instance-dir when using \"mo2\" "
                                 "mod manager type");
    }
  });
}

void initLogger(const filesystem::path &LOGPATH, const ParallaxGenCLIArgs &Args) {
  // Create loggers
  vector<spdlog::sink_ptr> Sinks;
  auto ConsoleSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
  Sinks.push_back(ConsoleSink);

  // Rotating file sink
  auto FileSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(LOGPATH.string(), MAX_LOG_SIZE, MAX_LOG_FILES);
  Sinks.push_back(FileSink); // TODO wide string support here
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
  // This is what keeps the console window open after the program exits until user input
  if (atexit(exitBlocking) != 0) {
    cerr << "Failed to register exitBlocking function\n";
    return 1;
  }

  // Find location of ParallaxGen.exe
  const filesystem::path ExePath = getExecutablePath().parent_path();

  // CLI Arguments
  ParallaxGenCLIArgs Args;
  CLI::App App{"ParallaxGen: Auto convert meshes to parallax meshes"};
  addArguments(App, Args, ExePath);

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
