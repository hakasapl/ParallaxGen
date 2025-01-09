#include <CLI/CLI.hpp>

#include <spdlog/spdlog.h>

#include <windows.h>

#include <cpptrace/from_current.hpp>

#include <string>
#include <unordered_set>

#include "ParallaxGen.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenWarnings.hpp"
#include "ParallaxGenRunner.hpp"

#include "patchers/Patcher.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherUpgradeParallaxToCM.hpp"
#include "patchers/PatcherVanillaParallax.hpp"
#include "patchers/PatcherParticleLightsToLP.hpp"

using namespace std;

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

struct PGToolsCLIArgs {
  int Verbosity = 0;
  bool Multithreading = true;
  bool GPUAcceleration = true;

  struct Patch {
    CLI::App *SubCommand = nullptr;
    unordered_set<string> Patchers;
    filesystem::path Source = ".";
    filesystem::path Output = "ParallaxGen_Output";
    bool MapTexturesFromMeshes = false;
    bool HighMem = false;
  } Patch;
};

void mainRunner(PGToolsCLIArgs &Args) {
  // Welcome Message
  spdlog::info("Welcome to PGTools version {}!", PARALLAXGEN_VERSION);

  // Get EXE path
  const auto ExePath = getExecutablePath().parent_path();

  // Test message if required
  if (PARALLAXGEN_TEST_VERSION > 0) {
    spdlog::warn("This is an EXPERIMENTAL development build of ParallaxGen: {} Test Build {}", PARALLAXGEN_VERSION,
                 PARALLAXGEN_TEST_VERSION);
  }

  // Check if patch subcommand was used
  if (Args.Patch.SubCommand->parsed()) {
    // Get current time to compare later
    auto StartTime = chrono::high_resolution_clock::now();
    long long TimeTaken = 0;

    Args.Patch.Source = filesystem::absolute(Args.Patch.Source);
    Args.Patch.Output = filesystem::absolute(Args.Patch.Output);

    auto PGD = ParallaxGenDirectory(Args.Patch.Source, Args.Patch.Output, nullptr);
    auto PGD3D = ParallaxGenD3D(&PGD, Args.Patch.Output, ExePath, Args.GPUAcceleration);
    auto PG = ParallaxGen(Args.Patch.Output, &PGD, &PGD3D, Args.Patch.Patchers.contains("optimize"));

    Patcher::loadStatics(PGD, PGD3D);
    ParallaxGenWarnings::init(&PGD, {});

    // Check if GPU needs to be initialized
    if (Args.GPUAcceleration) {
      PGD3D.initGPU();
    }

    // Create output directory
    try {
      filesystem::create_directories(Args.Patch.Output);
    } catch (const filesystem::filesystem_error &E) {
      spdlog::error("Failed to create output directory: {}", E.what());
      exit(1);
    }

    // If output dir is the same as data dir meshes might get overwritten
    if (filesystem::equivalent(Args.Patch.Output, PGD.getDataPath())) {
      spdlog::critical("Output directory cannot be the same directory as your data folder. "
                       "Exiting.");
      exit(1);
    }

    // delete existing output
    PG.deleteOutputDir();

    // Init file map
    PGD.populateFileMap(false);

    // Map files
    PGD.mapFiles({}, {}, {}, {}, Args.Patch.MapTexturesFromMeshes, Args.Multithreading, Args.Patch.HighMem);

    // Split patchers into names and options
    unordered_map<string, unordered_set<string>> PatcherDefs;
    for (const auto &Patcher : Args.Patch.Patchers) {
      auto OpenBracket = Patcher.find('[');
      auto CloseBracket = Patcher.find(']');
      if (OpenBracket == string::npos || CloseBracket == string::npos) {
        PatcherDefs[Patcher] = {};
        continue;
      }

      // Get substring between brackets
      auto Options = Patcher.substr(OpenBracket + 1, CloseBracket - OpenBracket - 1);
      // Split options by | into unordered set
      unordered_set<string> OptionSet;
      for (const auto &Option : Options | views::split('|')) {
        OptionSet.insert(string(Option.begin(), Option.end()));
      }

      // Add to set
      PatcherDefs[Patcher.substr(0, OpenBracket)] = OptionSet;
    }

    if (PatcherDefs.contains("complexmaterial")) {
      // Find CM maps
      spdlog::info("Finding complex material env maps");
      PGD3D.findCMMaps({});
      spdlog::info("Done finding complex material env maps");
    }

    // Create patcher factory
    PatcherUtil::PatcherSet Patchers;
    if (PatcherDefs.contains("parallax")) {
      Patchers.ShaderPatchers.emplace(PatcherVanillaParallax::getShaderType(), PatcherVanillaParallax::getFactory());
    }
    if (PatcherDefs.contains("complexmaterial")) {
      Patchers.ShaderPatchers.emplace(PatcherComplexMaterial::getShaderType(), PatcherComplexMaterial::getFactory());
      PatcherComplexMaterial::loadStatics(Args.Patch.Patchers.contains("disablemlp"), {});
    }
    if (PatcherDefs.contains("truepbr")) {
      Patchers.ShaderPatchers.emplace(PatcherTruePBR::getShaderType(), PatcherTruePBR::getFactory());
      PatcherTruePBR::loadStatics(PGD.getPBRJSONs());
      PatcherTruePBR::loadOptions(PatcherDefs["truepbr"]);
    }
    if (PatcherDefs.contains("parallaxtocm")) {
      Patchers.ShaderTransformPatchers[PatcherUpgradeParallaxToCM::getFromShader()].emplace(
          PatcherUpgradeParallaxToCM::getToShader(), PatcherUpgradeParallaxToCM::getFactory());
    }
    if (PatcherDefs.contains("particlelightstolp")) {
      Patchers.GlobalPatchers.emplace_back(PatcherParticleLightsToLP::getFactory());
    }

    PG.loadPatchers(Patchers);
    PG.patchMeshes(Args.Multithreading, false);

    // Finalize step
    if (PatcherDefs.contains("particlelightstolp")) {
      PatcherParticleLightsToLP::finalize();
    }

    // Release cached files, if any
    PGD.clearCache();

    // Check if dynamic cubemap file is needed
    if (Args.Patch.Patchers.contains("complexmaterial")) {
      // Install default cubemap file if needed
      static const filesystem::path DynCubeMapPath = "textures/cubemaps/dynamic1pxcubemap_black.dds";

      spdlog::info("Installing default dynamic cubemap file");

      // Create Directory
      const filesystem::path OutputCubemapPath = Args.Patch.Output / DynCubeMapPath.parent_path();
      filesystem::create_directories(OutputCubemapPath);

      filesystem::path AssetPath = filesystem::path(ExePath) / "assets/dynamic1pxcubemap_black_ENB.dds";
      filesystem::path OutputPath = filesystem::path(Args.Patch.Output) / DynCubeMapPath;

      // Move File
      filesystem::copy_file(AssetPath, OutputPath, filesystem::copy_options::overwrite_existing);
    }

    const auto EndTime = chrono::high_resolution_clock::now();
    TimeTaken += chrono::duration_cast<chrono::seconds>(EndTime - StartTime).count();

    spdlog::info("ParallaxGen took {} seconds to complete", TimeTaken);
  }
}

void addArguments(CLI::App &App, PGToolsCLIArgs &Args) {
  // Logging
  App.add_flag("-v", Args.Verbosity,
               "Verbosity level -v for DEBUG data or -vv for TRACE data "
               "(warning: TRACE data is very verbose)");
  App.add_flag("--no-multithreading", Args.Multithreading, "Disable multithreading");
  App.add_flag("--no-gpu-acceleration", Args.GPUAcceleration, "Disable GPU acceleration");

  Args.Patch.SubCommand = App.add_subcommand("patch", "Patch meshes");
  Args.Patch.SubCommand->add_option("patcher", Args.Patch.Patchers, "List of patchers to use")
      ->required()
      ->delimiter(',');
  Args.Patch.SubCommand->add_option("source", Args.Patch.Source, "Source directory")->default_str("");
  Args.Patch.SubCommand->add_option("output", Args.Patch.Output, "Output directory")->default_str("ParallaxGen_Output");
  Args.Patch.SubCommand->add_flag("--map-textures-from-meshes", Args.Patch.MapTexturesFromMeshes,
                                  "Map textures from meshes (default: false)");
  Args.Patch.SubCommand->add_flag("--high-mem", Args.Patch.HighMem, "High memory usage mode (default: false)");
}

auto main(int ArgC, char **ArgV) -> int {
// Block until enter only in debug mode
#ifdef _DEBUG
  cout << "Press ENTER to start (DEBUG mode)...";
  cin.get();
#endif

  SetConsoleOutputCP(CP_UTF8);

  // CLI Arguments
  PGToolsCLIArgs Args;
  CLI::App App{"PGTools: A collection of tools for ParallaxGen"};
  addArguments(App, Args);

  // Parse CLI Arguments (this is what exits on any validation issues)
  CLI11_PARSE(App, ArgC, ArgV);

  // Initialize Logger
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  // Set logging mode
  if (Args.Verbosity >= 1) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("DEBUG logging enabled");
  }

  if (Args.Verbosity >= 2) {
    spdlog::set_level(spdlog::level::trace);
    spdlog::trace("TRACE logging enabled");
  }

  // Main Runner (Catches all exceptions)
  CPPTRACE_TRY {
    mainRunner(Args);
  } CPPTRACE_CATCH(const exception& E) {
    ParallaxGenRunner::processException(E, cpptrace::from_current_exception().to_string());

    cout << "Press ENTER to abort...";
    cin.get();
    abort();
  }
}
