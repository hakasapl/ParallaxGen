#include "BethesdaGame.hpp"
#include "Logger.hpp"
#include "ModManagerDirectory.hpp"
#include "ParallaxGen.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenHandlers.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenRunner.hpp"
#include "ParallaxGenUI.hpp"
#include "ParallaxGenWarnings.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherDefault.hpp"
#include "patchers/PatcherFixMeshLighting.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherUpgradeParallaxToCM.hpp"
#include "patchers/PatcherUtil.hpp"
#include "patchers/PatcherVanillaParallax.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <CLI/CLI.hpp>

#include <cpptrace/from_current.hpp>

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

constexpr unsigned MAX_LOG_SIZE = 5242880;
constexpr unsigned MAX_LOG_FILES = 1000;

using namespace std;
struct ParallaxGenCLIArgs {
    int verbosity = 0;
    bool autostart = false;
    bool fullDump = false;
};

namespace {
auto deployAssets(const filesystem::path& outputDir, const filesystem::path& exePath) -> void
{
    // Install default cubemap file
    static const filesystem::path dynCubeMapPath = "textures/cubemaps/dynamic1pxcubemap_black.dds";

    Logger::info("Installing default dynamic cubemap file");

    // Create Directory
    const filesystem::path outputCubemapPath = outputDir / dynCubeMapPath.parent_path();
    filesystem::create_directories(outputCubemapPath);

    const filesystem::path assetPath = filesystem::path(exePath) / "assets/dynamic1pxcubemap_black.dds";
    const filesystem::path outputPath = filesystem::path(outputDir) / dynCubeMapPath;

    // Move File
    filesystem::copy_file(assetPath, outputPath, filesystem::copy_options::overwrite_existing);
}

void mainRunner(ParallaxGenCLIArgs& args, const filesystem::path& exePath)
{
    // Welcome Message
    Logger::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);

    // Test message if required
    if (PARALLAXGEN_TEST_VERSION > 0) {
        Logger::warn("This is an EXPERIMENTAL development build of ParallaxGen: {} Test Build {}", PARALLAXGEN_VERSION,
            PARALLAXGEN_TEST_VERSION);
    }

    // Alpha message
    Logger::warn("ParallaxGen is currently in ALPHA. Please file detailed bug reports on nexus or github.");

    // Create cfg directory if it does not exist
    const filesystem::path cfgDir = exePath / "cfg";
    if (!filesystem::exists(cfgDir)) {
        Logger::info(L"There is no existing configuration. Creating config directory \"{}\"", cfgDir.wstring());
        filesystem::create_directories(cfgDir);
    }

    // Initialize ParallaxGenConfig
    ParallaxGenConfig::loadStatics(exePath);
    auto pgc = ParallaxGenConfig();
    pgc.loadConfig();

    // Initialize UI
    ParallaxGenUI::init();

    auto params = pgc.getParams();

    // Show launcher UI
    if (!args.autostart) {
        Logger::info("Showing launcher UI");
        params = ParallaxGenUI::showLauncher(pgc);
    }

    // Validate config
    vector<string> errors;
    if (!ParallaxGenConfig::validateParams(params, errors)) {
        // This should never happen because there is a frontend validation that would have to be bypassed
        for (const auto& error : errors) {
            Logger::error("{}", error);
        }
        Logger::critical("Validation errors were found. Exiting.");
    }

    // Print configuration parameters
    Logger::debug(L"Configuration Parameters:\n\n{}\n", params.getString());

    // print output location
    Logger::info(L"ParallaxGen output directory: {}", params.Output.dir.wstring());

    // Create relevant objects
    auto bg = BethesdaGame(params.Game.type, true, params.Game.dir);

    auto mmd = ModManagerDirectory(params.ModManager.type);
    auto pgd = ParallaxGenDirectory(&bg, params.Output.dir, &mmd);
    auto pgd3d = ParallaxGenD3D(&pgd, params.Output.dir, exePath);
    auto pg = ParallaxGen(params.Output.dir, &pgd, &pgd3d, params.PostPatcher.optimizeMeshes);

    Patcher::loadStatics(pgd, pgd3d);

    // Check if GPU needs to be initialized
    Logger::info("Initializing GPU");
    pgd3d.initGPU();

    //
    // Generation
    //

    // Get current time to compare later
    auto startTime = chrono::high_resolution_clock::now();
    long long timeTaken = 0;

    // Create output directory
    try {
        if (filesystem::create_directories(params.Output.dir)) {
            Logger::debug(L"Output directory created: {}", params.Output.dir.wstring());
        }
    } catch (const filesystem::filesystem_error& e) {
        Logger::error("Failed to create output directory: {}", e.what());
        exit(1);
    }

    // If output dir is the same as data dir meshes might get overwritten
    if (filesystem::equivalent(params.Output.dir, pgd.getDataPath())) {
        Logger::critical("Output directory cannot be the same directory as your data folder. "
                         "Exiting.");
    }

    // If output dir is a subdirectory of data dir vfs issues can occur
    if (boost::istarts_with(params.Output.dir.wstring(), bg.getGameDataPath().wstring() + "\\")) {
        Logger::critical("Output directory cannot be a subdirectory of your data folder. Exiting.");
    }

    // Check if dyndolod.esp exists
    const auto activePlugins = bg.getActivePlugins(false, true);
    if (ranges::find(activePlugins, L"dyndolod.esp") != activePlugins.end()) {
        Logger::critical(
            "DynDoLOD and TexGen outputs must be disabled prior to running ParallaxGen. It is recommended to "
            "generate LODs after running ParallaxGen with the ParallaxGen output enabled.");
    }

    // Init PGP library
    if (params.Processing.pluginPatching) {
        Logger::info("Initializing plugin patching");
        ParallaxGenPlugin::loadStatics(&pgd);
        ParallaxGenPlugin::initialize(bg, exePath);
        ParallaxGenPlugin::populateObjs();
    }

    // Populate file map from data directory
    if (params.ModManager.type == ModManagerDirectory::ModManagerType::MODORGANIZER2
        && !params.ModManager.mo2InstanceDir.empty() && !params.ModManager.mo2Profile.empty()) {
        // MO2
        mmd.populateModFileMapMO2(params.ModManager.mo2InstanceDir, params.ModManager.mo2Profile, params.Output.dir);
    } else if (params.ModManager.type == ModManagerDirectory::ModManagerType::VORTEX) {
        // Vortex
        mmd.populateModFileMapVortex(bg.getGameDataPath());
    }

    // delete existing output
    pg.deleteOutputDir();

    // Check if ParallaxGen output already exists in data directory
    const filesystem::path pgStateFilePath = bg.getGameDataPath() / ParallaxGen::getDiffJSONName();
    if (filesystem::exists(pgStateFilePath)) {
        Logger::critical("ParallaxGen meshes exist in your data directory, please delete before "
                         "re-running.");
    }

    // Init file map
    pgd.populateFileMap(params.Processing.bsa);

    // Map files
    pgd.mapFiles(params.MeshRules.blockList, params.MeshRules.allowList, params.TextureRules.textureMaps,
        params.TextureRules.vanillaBSAList, params.Processing.mapFromMeshes, params.Processing.multithread,
        params.Processing.highMem);

    // Find CM maps
    if (params.ShaderPatcher.complexMaterial) {
        pgd3d.findCMMaps(params.TextureRules.vanillaBSAList);
    }

    // Create patcher factory
    PatcherUtil::PatcherMeshSet meshPatchers;
    if (params.PrePatcher.fixMeshLighting) {
        Logger::debug("Adding Mesh Lighting Fix pre-patcher");
        meshPatchers.prePatchers.emplace_back(PatcherFixMeshLighting::getFactory());
    }
    meshPatchers.shaderPatchers.emplace(PatcherDefault::getShaderType(), PatcherDefault::getFactory());
    if (params.ShaderPatcher.parallax) {
        Logger::debug("Adding Parallax shader patcher");
        meshPatchers.shaderPatchers.emplace(
            PatcherVanillaParallax::getShaderType(), PatcherVanillaParallax::getFactory());
    }
    if (params.ShaderPatcher.complexMaterial) {
        Logger::debug("Adding Complex Material shader patcher");
        meshPatchers.shaderPatchers.emplace(
            PatcherComplexMaterial::getShaderType(), PatcherComplexMaterial::getFactory());
        PatcherComplexMaterial::loadStatics(
            params.PrePatcher.disableMLP, params.ShaderPatcher.ShaderPatcherComplexMaterial.listsDyncubemapBlocklist);
    }
    if (params.ShaderPatcher.truePBR) {
        Logger::debug("Adding True PBR shader patcher");
        meshPatchers.shaderPatchers.emplace(PatcherTruePBR::getShaderType(), PatcherTruePBR::getFactory());
        PatcherTruePBR::loadStatics(pgd.getPBRJSONs());
    }
    if (params.ShaderTransforms.parallaxToCM) {
        Logger::debug("Adding Parallax to Complex Material shader transform patcher");
        meshPatchers.shaderTransformPatchers[PatcherUpgradeParallaxToCM::getFromShader()].emplace(
            PatcherUpgradeParallaxToCM::getToShader(), PatcherUpgradeParallaxToCM::getFactory());
    }

    const PatcherUtil::PatcherTextureSet texPatchers;
    pg.loadPatchers(meshPatchers, texPatchers);

    if (params.ModManager.type != ModManagerDirectory::ModManagerType::NONE) {
        // Check if MO2 is used and MO2 use order is checked
        if (params.ModManager.type == ModManagerDirectory::ModManagerType::MODORGANIZER2
            && params.ModManager.mo2UseOrder) {
            // Get mod order from MO2
            const auto& modOrder = mmd.getInferredOrder();
            pgc.setModOrder(modOrder);
        } else {
            // Find conflicts
            const auto modConflicts
                = pg.findModConflicts(params.Processing.multithread, params.Processing.pluginPatching);
            const auto existingOrder = pgc.getModOrder();

            if (!modConflicts.empty()) {
                // pause timer for UI
                timeTaken
                    += chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - startTime).count();

                // Select mod order
                Logger::info("Mod conflicts found. Showing mod order dialog.");
                auto selectedOrder = ParallaxGenUI::selectModOrder(modConflicts, existingOrder);
                startTime = chrono::high_resolution_clock::now();

                pgc.setModOrder(selectedOrder);
            }
        }

        pgc.saveUserConfig();
    }

    // Patch meshes if set
    auto modPriorityMap = pgc.getModPriorityMap();
    pg.loadModPriorityMap(&modPriorityMap);
    ParallaxGenWarnings::init(&pgd, &modPriorityMap);
    pg.patch(params.Processing.multithread, params.Processing.pluginPatching);

    // Release cached files, if any
    pgd.clearCache();

    // Write plugin
    if (params.Processing.pluginPatching) {
        Logger::info("Saving Plugins...");
        ParallaxGenPlugin::savePlugin(params.Output.dir, params.Processing.pluginESMify);
    }

    deployAssets(params.Output.dir, exePath);

    // archive
    if (params.Output.zip) {
        pg.zipMeshes();
        pg.deleteOutputDir(false);
    }

    const auto endTime = chrono::high_resolution_clock::now();
    timeTaken += chrono::duration_cast<chrono::seconds>(endTime - startTime).count();

    Logger::info("ParallaxGen took {} seconds to complete (does not include time in user interface)", timeTaken);
}

void addArguments(CLI::App& app, ParallaxGenCLIArgs& args)
{
    // Logging
    app.add_flag("-v", args.verbosity,
        "Verbosity level -v for DEBUG data or -vv for TRACE data "
        "(warning: TRACE data is very verbose)");
    app.add_flag("--autostart", args.autostart, "Start generation without user input");
    app.add_flag("--full-dump", args.fullDump, "Save all memory to crash dumps");
}

void initLogger(const filesystem::path& logpath, const ParallaxGenCLIArgs& args)
{
    // Create loggers
    vector<spdlog::sink_ptr> sinks;
    auto consoleSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(consoleSink);

    // Rotating file sink
    auto fileSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logpath.wstring(), MAX_LOG_SIZE, MAX_LOG_FILES);
    sinks.push_back(fileSink);
    auto logger = make_shared<spdlog::logger>("ParallaxGen", sinks.begin(), sinks.end());

    // register logger parameters
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    // Set logging mode
    if (args.verbosity >= 1) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        spdlog::debug("DEBUG logging enabled");
    }

    if (args.verbosity >= 2) {
        spdlog::set_level(spdlog::level::trace);
        consoleSink->set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::trace);
        spdlog::trace("TRACE logging enabled");
    }
}
}

auto main(int ArgC, char** ArgV) -> int
{
// Block until enter only in debug mode
#ifdef _DEBUG
    cout << "Press ENTER to start (DEBUG mode)...";
    cin.get();
#endif

    SetUnhandledExceptionFilter(ParallaxGenHandlers::customExceptionHandler);

    // This is what keeps the console window open after the program exits until user input
    if (atexit(ParallaxGenHandlers::exitBlocking) != 0) {
        cerr << "Failed to register exitBlocking function\n";
        return 1;
    }

    SetConsoleOutputCP(CP_UTF8);

    // Find location of ParallaxGen.exe
    const filesystem::path exePath = ParallaxGenHandlers::getExePath().parent_path();

    // CLI Arguments
    ParallaxGenCLIArgs args;
    CLI::App app { "ParallaxGen: Auto convert meshes to parallax meshes" };
    addArguments(app, args);

    // Parse CLI Arguments (this is what exits on any validation issues)
    CLI11_PARSE(app, ArgC, ArgV);

    // Set dump type
    ParallaxGenHandlers::setDumpType(args.fullDump);

    // Initialize logger
    const filesystem::path logDir = exePath / "log";
    // delete old logs
    if (filesystem::exists(logDir)) {
        try {
            // Only delete files that are .log and start with ParallaxGen
            for (const auto& entry : filesystem::directory_iterator(logDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".log"
                    && entry.path().filename().wstring().starts_with(L"ParallaxGen")) {
                    filesystem::remove(entry.path());
                }
            }
        } catch (const filesystem::filesystem_error& e) {
            cerr << "Failed to delete old logs: " << e.what() << "\n";
            return 1;
        }
    }

    const filesystem::path logPath = logDir / "ParallaxGen.log";
    initLogger(logPath, args);

    // Main Runner (Catches all exceptions)
    CPPTRACE_TRY { mainRunner(args, exePath); }
    CPPTRACE_CATCH(const exception& e)
    {
        ParallaxGenRunner::processException(e, cpptrace::from_current_exception().to_string());

        cout << "Press ENTER to abort...";
        cin.get();
        abort();
    }
}
