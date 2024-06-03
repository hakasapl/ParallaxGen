#include <windows.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "BethesdaGame/BethesdaGame.hpp"
#include "ParallaxGen/ParallaxGen.hpp"
#include "ParallaxGenUtil/ParallaxGenUtil.hpp"
#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

using namespace std;
using namespace ParallaxGenUtil;
namespace fs = std::filesystem;

fs::path getExecutablePath() {
    char buffer[MAX_PATH];
    if (GetModuleFileName(NULL, buffer, MAX_PATH) == 0) {
        spdlog::critical("Unable to locate ParallaxGen.exe: {}", GetLastError());
        exitWithUserInput(1);
    }

    try {
        fs::path out_path = fs::path(buffer);

        if (fs::exists(out_path)) {
            return out_path;
        }
        else {
            spdlog::critical("Located ParallaxGen.exe path is invalid: {}", out_path.string());
            exitWithUserInput(1);
        }
    }
    catch(const fs::filesystem_error& ex) {
        spdlog::critical("Unable to locate ParallaxGen.exe: {}", ex.what());
        exitWithUserInput(1);
    }

    return fs::path();
}

void addArguments(CLI::App& app, int& verbosity, fs::path& game_dir, string& game_type, bool& no_zip, bool& no_cleanup, bool& ignore_parallax, bool& ignore_complex_material) {
    app.add_flag("-v", verbosity, "Verbosity level -v for DEBUG data or -vv for TRACE data (warning: TRACE data is very verbose)");
    app.add_option("-d,--game-dir", game_dir, "Manually specify of Skyrim directory");
    app.add_option("-g,--game-type", game_type, "Specify game type [skyrimse, skyrim, or skyrimvr]");
    app.add_flag("--no-zip", no_zip, "Don't zip the output meshes");
    app.add_flag("--no-cleanup", no_cleanup, "Don't delete generated meshes after zipping");
    app.add_flag("--ignore-parallax", ignore_parallax, "Don't generate any parallax meshes");
    app.add_flag("--enable-complex-material", ignore_complex_material, "Generate any complex material meshes (Experimental!)");
}

int main(int argc, char** argv) {
    // get program location
    const fs::path output_dir = getExecutablePath().parent_path();

    // Create loggers
    vector<spdlog::sink_ptr> sinks;
    try {
        auto console_sink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto basic_sink = make_shared<spdlog::sinks::basic_file_sink_mt>(output_dir.string() + "/ParallaxGen.log", true);
        sinks = {console_sink, basic_sink};
    }
    catch (const spdlog::spdlog_ex& ex) {
        spdlog::critical("Error creating logging objects: {}", ex.what());
        exitWithUserInput(1);
    }

    auto logger = make_shared<spdlog::logger>("ParallaxGen", sinks.begin(), sinks.end());

    // register logger parameters
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);

    //
    // CLI Arguments
    //
    int verbosity = 0;
    fs::path game_dir;
    string game_type = "skyrimse";
    bool no_zip = false;
    bool no_cleanup = false;
    bool ignore_parallax = false;
    bool ignore_complex_material = false;  // this is prefixed with ignore because eventually this should be an ignore option once stable

    CLI::App app{ "ParallaxGen: Auto convert meshes to parallax meshes" };
    addArguments(app, verbosity, game_dir, game_type, no_zip, no_cleanup, ignore_parallax, ignore_complex_material);
    CLI11_PARSE(app, argc, argv);

    // welcome message
    spdlog::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);

    // CLI argument validation
    if (game_type != "skyrimse" && game_type != "skyrim" && game_type != "skyrimvr") {
        spdlog::critical("Invalid game type specified. Please specify 'skyrimse', 'skyrim', or 'skyrimvr'");
        exitWithUserInput(1);
    }

    if (ignore_parallax && !ignore_complex_material) {
        spdlog::critical("If ignore-parallax is set, enable-complex-material must be set, otherwise there is nothing to do");
        exitWithUserInput(1);
    }

    // Set logging mode
    if (verbosity >= 1) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        spdlog::debug("DEBUG logging enabled");
    }
    
    if (verbosity >= 2) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
        spdlog::trace("TRACE logging enabled");
    }

    // print mesh output location
    fs::path mesh_output_dir = output_dir / "ParallaxGen_Output";
    spdlog::info(L"Mesh output directory: {}", mesh_output_dir.wstring());

    // Create bethesda game object
    BethesdaGame::GameType gameType;
    if (game_type == "skyrimse") {
        gameType = BethesdaGame::GameType::SKYRIM_SE;
    }
    else if (game_type == "skyrim") {
        gameType = BethesdaGame::GameType::SKYRIM;
    }
    else if (game_type == "skyrimvr") {
        gameType = BethesdaGame::GameType::SKYRIM_VR;
    }

	BethesdaGame bg = BethesdaGame(gameType, game_dir, true);
    ParallaxGenDirectory pgd = ParallaxGenDirectory(bg);
    ParallaxGen pg = ParallaxGen(mesh_output_dir, &pgd);

    if (fs::exists(bg.getGameDataPath() / ParallaxGen::parallax_state_file)) {
        spdlog::critical("ParallaxGen meshes exist in your data directory, please delete before re-running.");
        exitWithUserInput(1);
    }

    // User Input to Continue
    cout << "Press ENTER to start mesh generation...";
    cin.get();

    // delete existing output
    pg.deleteOutputDir();

    // Populate file map from data directory
    pgd.populateFileMap();

    // Build file vectors
    vector<fs::path> heightMaps;
    if (!ignore_parallax) {
        heightMaps = pgd.findHeightMaps();
    }

    vector<fs::path> complexMaterialMaps;
    if (ignore_complex_material) {
        complexMaterialMaps = pgd.findComplexMaterialMaps();
    }

    vector<fs::path> meshes = pgd.findMeshes();

    // Patch meshes
    pg.patchMeshes(meshes, heightMaps, complexMaterialMaps);

    spdlog::info("ParallaxGen has finished generating meshes.");

    // archive
    if (!no_zip) {
        pg.zipMeshes();
    }

    // cleanup
    if (!no_cleanup) {
        pg.deleteMeshes();
    }

    // Close Console
	exitWithUserInput(0);
}
