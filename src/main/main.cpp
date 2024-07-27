#include <windows.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/stacktrace.hpp>

#include "BethesdaGame/BethesdaGame.hpp"
#include "ParallaxGen/ParallaxGen.hpp"
#include "ParallaxGenUtil/ParallaxGenUtil.hpp"
#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"
#include "ParallaxGenD3D/ParallaxGenD3D.hpp"

using namespace std;
using namespace ParallaxGenUtil;

filesystem::path getExecutablePath() {
    char buffer[MAX_PATH];
    if (GetModuleFileName(NULL, buffer, MAX_PATH) == 0) {
        cerr << "Error getting executable path: " << GetLastError() << "\n";
        exitWithUserInput(1);
    }

    filesystem::path out_path = filesystem::path(buffer);

    if (filesystem::exists(out_path)) {
        return out_path;
    }
    else {
        cerr << "Error getting executable path: path does not exist\n";
        exitWithUserInput(1);
    }

    return filesystem::path();
}

void initLogger(filesystem::path LOG_PATH)
{
    // Create loggers
    vector<spdlog::sink_ptr> sinks;
    sinks.push_back(make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(make_shared<spdlog::sinks::basic_file_sink_mt>(LOG_PATH.string(), true));  // TODO wide string support here
    auto logger = make_shared<spdlog::logger>("ParallaxGen", sinks.begin(), sinks.end());

    // register logger parameters
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
}

void addArguments(
    CLI::App& app,
    int& verbosity,
    filesystem::path& game_dir,
    string& game_type,
    filesystem::path& output_dir,
    bool& upgrade_shaders,
    bool& optimize_meshes,
    bool& no_zip,
    bool& no_cleanup,
    bool& ignore_parallax,
    bool& ignore_complex_material,
    bool& ignore_truepbr,
    const string& AVAILABLE_GAME_TYPES_STR
    )
{
    app.add_flag("-v", verbosity, "Verbosity level -v for DEBUG data or -vv for TRACE data (warning: TRACE data is very verbose)");
    app.add_option("-d,--game-dir", game_dir, "Manually specify game directory");
    app.add_option("-g,--game-type", game_type, "Specify game type [" + AVAILABLE_GAME_TYPES_STR + "]");
    app.add_option("-o,--output-dir", output_dir, "Manually specify output directory");
    app.add_flag("--upgrade-shaders", upgrade_shaders, "Upgrade shaders to a better version whenever possible");
    app.add_flag("--optimize-meshes", optimize_meshes, "Optimize meshes before saving them");
    app.add_flag("--no-zip", no_zip, "Don't zip the output meshes (also enables --no-cleanup)");
    app.add_flag("--no-cleanup", no_cleanup, "Don't delete generated meshes after zipping");
    app.add_flag("--ignore-parallax", ignore_parallax, "Don't generate any parallax meshes");
    app.add_flag("--ignore-complex-material", ignore_complex_material, "Don't generate any complex material meshes");
    app.add_flag("--ignore-truepbr", ignore_truepbr, "Don't apply any TruePBR configs in the load order");
}

// Store game type strings and their corresponding BethesdaGame::GameType enum values
// This also determines the CLI argument help text (the key values)
const unordered_map<string, BethesdaGame::GameType> GAME_TYPE_MAP = {
    {"skyrimse", BethesdaGame::GameType::SKYRIM_SE},
    {"skyrimgog", BethesdaGame::GameType::SKYRIM_GOG},
    {"skyrim", BethesdaGame::GameType::SKYRIM},
    {"skyrimvr", BethesdaGame::GameType::SKYRIM_VR},
    {"enderal", BethesdaGame::GameType::ENDERAL},
    {"enderalse", BethesdaGame::GameType::ENDERAL_SE}
};

void mainRunner(int argc, char** argv)
{
    // Find location of ParallaxGen.exe
    const filesystem::path EXE_PATH = getExecutablePath().parent_path();

    // Initialize logger
    const filesystem::path LOG_PATH = EXE_PATH / "ParallaxGen.log";
    initLogger(LOG_PATH);

    //
    // CLI Arguments
    //
    const string STARTUP_CLI_ARGS = boost::join(vector<string>(argv + 1, argv + argc), " ");

    // Define game type vars
    vector<string> AVAILABLE_GAME_TYPES;
    for (const auto& pair : GAME_TYPE_MAP) {
        AVAILABLE_GAME_TYPES.push_back(pair.first);
    }
    const string AVAILABLE_GAME_TYPES_STR = boost::join(AVAILABLE_GAME_TYPES, ", ");

    // Vars that store CLI argument values
    // Default values are defined here
    int verbosity = 0;
    filesystem::path game_dir;
    string game_type = "skyrimse";
    filesystem::path output_dir = EXE_PATH / "ParallaxGen_Output";
    bool upgrade_shaders = false;
    bool optimize_meshes = false;
    bool no_zip = false;
    bool no_cleanup = false;
    bool ignore_parallax = false;
    bool ignore_complex_material = false;
    bool ignore_truepbr = false;

    // Parse CLI arguments
    CLI::App app{ "ParallaxGen: Auto convert meshes to parallax meshes" };
    addArguments(app, verbosity, game_dir, game_type, output_dir, upgrade_shaders, optimize_meshes, no_zip, no_cleanup, ignore_parallax, ignore_complex_material, ignore_truepbr, AVAILABLE_GAME_TYPES_STR);

    // Check if arguments are valid, otherwise print error to user
    try {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e) {
        // TODO --help doesn't work correctly with this
        spdlog::critical("Arguments Invalid: {}\n{}", e.what(), app.help());
        exitWithUserInput(1);
    }

    //
    // Welcome Message
    //
    spdlog::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);
    spdlog::info("Arguments Supplied: {}", STARTUP_CLI_ARGS);

    //
    // Argument validation
    //

    // Check if game_type exists in allowed_game_types
    if (GAME_TYPE_MAP.find(game_type) == GAME_TYPE_MAP.end()){
        spdlog::critical("Invalid game type (-g) specified: {}", game_type);
        exitWithUserInput(1);
    }

    // Check if there is actually anything to do
    if (ignore_parallax && ignore_complex_material && ignore_truepbr && !upgrade_shaders) {
        spdlog::critical("There is nothing to do if both --ignore-parallax and --ignore-complex-material are set and --upgrade-shaders is not set.");
        exitWithUserInput(1);
    }

    // If --no-zip is set, also set --no-cleanup
    if (no_zip) {
        no_cleanup = true;
    }

    // Set logging mode
    if (verbosity == 1) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        spdlog::debug("DEBUG logging enabled");
    }

    if (verbosity >= 2) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
        spdlog::trace("TRACE logging enabled");
    }

    // If true, NIF patching occurs
    bool patch_meshes = !ignore_parallax || !ignore_complex_material || !ignore_truepbr;

    //
    // Init
    //

    // print output location
    spdlog::info(L"ParallaxGen output directory (the contents will be deleted if you start generation!): {}", output_dir.wstring());

    // Create bethesda game type object
    BethesdaGame::GameType bg_type = GAME_TYPE_MAP.at(game_type);

    // Create relevant objects
	BethesdaGame bg = BethesdaGame(bg_type, game_dir, true);
    ParallaxGenDirectory pgd = ParallaxGenDirectory(bg);
    ParallaxGenD3D pgd3d = ParallaxGenD3D(&pgd, output_dir, EXE_PATH);
    ParallaxGen pg = ParallaxGen(output_dir, &pgd, &pgd3d, optimize_meshes, ignore_parallax, ignore_complex_material, ignore_truepbr);

    // Check if GPU needs to be initialized
    if (upgrade_shaders) {
        pgd3d.initGPU();
    }

    // Check if ParallaxGen output already exists in data directory
    const filesystem::path PARALLAXGEN_STATE_FILE_PATH = bg.getGameDataPath() / ParallaxGen::parallax_state_file;
    if (filesystem::exists(PARALLAXGEN_STATE_FILE_PATH)) {
        spdlog::critical("ParallaxGen meshes exist in your data directory, please delete before re-running.");
        exitWithUserInput(1);
    }

    // Create output directory
    try {
        filesystem::create_directories(output_dir);
    } catch (const filesystem::filesystem_error& ex) {
        spdlog::error("Failed to create output directory: {}", ex.what());
        exitWithUserInput(1);
    }

    // If output dir is the same as data dir meshes might get overwritten
	if (filesystem::equivalent(output_dir, pgd.getDataPath())) {
		spdlog::critical("Output directory cannot be the same directory as your data folder. Exiting.");
		exitWithUserInput(1);
	}

    //
    // Generation
    //

    // User Input to Continue
    cout << "Press ENTER to start ParallaxGen...";
    cin.get();

    // delete existing output
    pg.deleteOutputDir();
    pg.initOutputDir();

    // Populate file map from data directory
    pgd.populateFileMap();

    // Install default cubemap file if needed
    if (!ignore_complex_material) {
        // install default cubemap file if needed
        if (!pgd.defCubemapExists()) {
            spdlog::info("Installing default cubemap file");

            // Create Directory
            filesystem::path output_cubemap_dir = output_dir / ParallaxGenDirectory::default_cubemap_path.parent_path();
            filesystem::create_directories(output_cubemap_dir);

            boost::filesystem::path asset_path = boost::filesystem::path(EXE_PATH) / L"assets/dynamic1pxcubemap_black_ENB.dds";
            boost::filesystem::path output_path = boost::filesystem::path(output_dir) / ParallaxGenDirectory::default_cubemap_path;

            // Move File
            boost::filesystem::copy_file(asset_path, output_path, boost::filesystem::copy_options::overwrite_existing);
        }
    }

    // Build file vectors
    if (!ignore_parallax || (ignore_parallax && upgrade_shaders)) {
        pgd.findHeightMaps();
    }

    if (!ignore_complex_material) {
        pgd.findComplexMaterialMaps();
    }

    if (!ignore_truepbr) {
        pgd.findTruePBRConfigs();
    }

    // Upgrade shaders if set
    if (upgrade_shaders) {
        pg.upgradeShaders();
    }

    // Patch meshes if set
    if (patch_meshes) {
        pgd.findMeshes();
        pg.patchMeshes();
    }

    spdlog::info("ParallaxGen has finished generating meshes.");

    // archive
    if (!no_zip) {
        pg.zipMeshes();
    }

    // cleanup
    if (!no_cleanup) {
        pg.deleteMeshes();
    }
}

int main(int argc, char** argv) {
    try {
        mainRunner(argc, argv);
        exitWithUserInput(0);
    } catch (const exception& ex) {
        spdlog::critical("An unhandled exception occurred (Please provide this entire message in your bug report).\n\nException type: {}\nMessage: {}\nStack Trace: \n{}", typeid(ex).name(), ex.what(), boost::stacktrace::to_string(boost::stacktrace::stacktrace()));
        exitWithUserInput(1);
    }
}
