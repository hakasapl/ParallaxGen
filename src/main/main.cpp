#include <windows.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <boost/algorithm/string.hpp>

#include "BethesdaGame/BethesdaGame.hpp"
#include "ParallaxGen/ParallaxGen.hpp"
#include "ParallaxGenUtil/ParallaxGenUtil.hpp"
#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"
#include "ParallaxGenD3D/ParallaxGenD3D.hpp"

using namespace std;
using namespace ParallaxGenUtil;
namespace fs = std::filesystem;

fs::path getExecutablePath() {
    char buffer[MAX_PATH];
    if (GetModuleFileName(NULL, buffer, MAX_PATH) == 0) {
        cout << "Error getting executable path: " << GetLastError() << "\n";
        exitWithUserInput(1);
    }

    try {
        fs::path out_path = fs::path(buffer);

        if (fs::exists(out_path)) {
            return out_path;
        }
        else {
            cout << "Error getting executable path: path does not exist\n";
            exitWithUserInput(1);
        }
    }
    catch(const fs::filesystem_error& ex) {
        cout << "Error getting executable path: " << ex.what() << "\n";
        exitWithUserInput(1);
    }

    return fs::path();
}

void addArguments(
    CLI::App& app,
    int& verbosity,
    fs::path& game_dir,
    string& game_type,
    fs::path& output_dir,
    bool& optimize_meshes,
    bool& no_zip,
    bool& no_cleanup,
    bool& ignore_parallax,
    bool& ignore_complex_material
    )
{
    app.add_flag("-v", verbosity, "Verbosity level -v for DEBUG data or -vv for TRACE data (warning: TRACE data is very verbose)");
    app.add_option("-d,--game-dir", game_dir, "Manually specify of Skyrim directory");
    app.add_option("-g,--game-type", game_type, "Specify game type [skyrimse, skyrim, skyrimvr, enderal, or enderalse]");
    app.add_option("-o,--output-dir", output_dir, "Manually specify output directory");
    app.add_flag("--optimize-meshes", optimize_meshes, "Optimize meshes before saving them");
    app.add_flag("--no-zip", no_zip, "Don't zip the output meshes (also enables --no-cleanup)");
    app.add_flag("--no-cleanup", no_cleanup, "Don't delete generated meshes after zipping");
    app.add_flag("--ignore-parallax", ignore_parallax, "Don't generate any parallax meshes");
    app.add_flag("--ignore-complex-material", ignore_complex_material, "Don't generate any complex material meshes");
}

int main(int argc, char** argv) {
    fs::path EXE_PATH = getExecutablePath().parent_path();

    // Create loggers
    vector<spdlog::sink_ptr> sinks;
    sinks.push_back(make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(make_shared<spdlog::sinks::basic_file_sink_mt>(EXE_PATH.string() + "/ParallaxGen.log", true));
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
    fs::path output_dir = EXE_PATH / "ParallaxGen_Output";
    bool optimize_meshes = false;
    bool no_zip = false;
    bool no_cleanup = false;
    bool ignore_parallax = false;
    bool ignore_complex_material = false;  // this is prefixed with ignore because eventually this should be an ignore option once stable

    CLI::App app{ "ParallaxGen: Auto convert meshes to parallax meshes" };
    addArguments(app, verbosity, game_dir, game_type, output_dir, optimize_meshes, no_zip, no_cleanup, ignore_parallax, ignore_complex_material);
    CLI11_PARSE(app, argc, argv);

    //! TODO print CLI args here, also don't close the application if CLI args are wrong

    // welcome message
    spdlog::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);

    // CLI argument validation
    if (game_type != "skyrimse" && game_type != "skyrim" && game_type != "skyrimvr" && game_type != "enderal" && game_type != "enderalse") {
        spdlog::critical("Invalid game type specified. Please specify 'skyrimse', 'skyrim', or 'skyrimvr'");
        exitWithUserInput(1);
    }

    if (ignore_parallax && ignore_complex_material) {
        spdlog::critical("If ignore-parallax is set, enable-complex-material must be set, otherwise there is nothing to do");
        exitWithUserInput(1);
    }

    if (no_zip) {
        no_cleanup = true;
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
    spdlog::info(L"Mesh output directory: {}", output_dir.wstring());

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
    else if (game_type == "enderal") {
        gameType = BethesdaGame::GameType::ENDERAL;
    }
    else if (game_type == "enderalse") {
        gameType = BethesdaGame::GameType::ENDERAL_SE;
    }

	BethesdaGame bg = BethesdaGame(gameType, game_dir, true);
    ParallaxGenDirectory pgd = ParallaxGenDirectory(bg);
    ParallaxGenD3D pgd3d = ParallaxGenD3D(&pgd);
    ParallaxGen pg = ParallaxGen(output_dir, &pgd, &pgd3d, optimize_meshes);

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
    if (!ignore_complex_material) {
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
