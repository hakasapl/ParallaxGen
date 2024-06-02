#include <iostream>
#include <fstream>
#include <string>
#include <bsa/tes4.hpp>
#include <cstdio>
#include <filesystem>
#include <tuple>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "BethesdaGame/BethesdaGame.hpp"
#include "ParallaxGenUtil/ParallaxGenUtil.hpp"
#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

using namespace std;
using namespace ParallaxGenUtil;
namespace fs = std::filesystem;

//todo: blacklist files that can be defined by mods
//todo: ability to specify multiple suffixes for height/env map?

static int processArguments(int argc, char** argv, bool& debug_mode, fs::path& data_dir, string& game_type, bool& ignore_parallax, bool& ignore_complex_material) {
    CLI::App app{ "ParallaxGen: Generate parallax meshes for your parallax textures" };

    app.add_flag("--debug", debug_mode, "Enable debug output in console");
    app.add_option("-d,--data-dir", data_dir, "Manually specify of Skyrim data directory (ie. <Skyrim SE Directory>/Data)");
    app.add_option("-g,--game-type", game_type, "Specify game type [skyrimse, skyrim, or skyrimvr]");
    app.add_flag("--ignore-parallax", ignore_parallax, "Don't generate any parallax meshes");
    app.add_flag("--ignore-complex-material", ignore_complex_material, "Don't generate any complex material meshes");

    CLI11_PARSE(app, argc, argv);
    return 0;
}

int main(int argc, char** argv) {
    spdlog::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);

    //
    // CLI Arguments
    //
    bool debug_mode = false;
    fs::path data_dir;
    string game_type = "skyrimse";
    bool ignore_parallax = false;
    bool ignore_complex_material = false;
    processArguments(argc, argv, debug_mode, data_dir, game_type, ignore_parallax, ignore_complex_material);

    // arg validation
    if (!fs::exists(data_dir) || !fs::is_directory(data_dir)) {
        spdlog::critical("Data directory does not exist. Please specify a valid directory.");
        exitWithUserInput(1);
    }

    if (game_type != "skyrimse" && game_type != "skyrim" && game_type != "skyrimvr") {
        spdlog::critical("Invalid game type specified. Please specify 'skyrimse', 'skyrim', or 'skyrimvr'");
        exitWithUserInput(1);
    }

    if (ignore_parallax && ignore_complex_material) {
        spdlog::critical("Both ignore-parallax and ignore-complex-material flags are set. Nothing to do.");
        exitWithUserInput(1);
    }

    // Set logging mode
    if (debug_mode) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug mode is ON");
    }
    else {
        spdlog::set_level(spdlog::level::info);
    }

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

	BethesdaGame bg = BethesdaGame(gameType, data_dir);

    // check that we're not in data folder as the PWD
    if (fs::current_path() == bg.getGameDataPath()) {
        spdlog::critical("The current working directory is set to the game data folder. This is dangerous because it could potentially overwrite game files. Please run this program from a different directory. (If using MO2 you can set the working directory to an empty 'ParallaxGen_Output' mod)");
        exitWithUserInput(1);
    }

    ParallaxGenDirectory pgd = ParallaxGenDirectory(bg, ignore_parallax, ignore_complex_material);

    // User Input to Continue
    cout << "Press ENTER to start mesh generation...";
    cin.get();

    // Populate file map from data directory
    pgd.populateFileMap();

    // Build file vectors
    pgd.buildFileVectors();

    // Patch meshes
    pgd.patchMeshes();

    // Close Console
	exitWithUserInput(0);
}
