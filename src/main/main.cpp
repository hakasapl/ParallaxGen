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

static int processArguments(int argc, char** argv, bool& debug_mode) {
    CLI::App app{ "ParallaxGen: Generate parallax meshes for your parallax textures" };

    app.add_flag("--debug", debug_mode, "Enable debug output in console");

    CLI11_PARSE(app, argc, argv);
    return 0;
}

int main(int argc, char** argv) {
    spdlog::info("Welcome to ParallaxGen!");

    //
    // CLI Arguments
    //
    bool debug_mode = false;
    processArguments(argc, argv, debug_mode);

    // Set logging mode
    if (debug_mode) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug mode is ON");
    }
    else {
        spdlog::set_level(spdlog::level::info);
    }

    // User Input to Continue
    cout << "Press ENTER to start mesh generation...";
    cin.get();

	BethesdaGame bg = BethesdaGame(BethesdaGame::GameType::SKYRIM_SE, "");
	ParallaxGenDirectory pgd = ParallaxGenDirectory(bg);

    /*
	BethesdaDirectory bdi = BethesdaDirectory(bg, true);
    vector<std::byte> test_cont = bdi.getFile(fs::path("meshes/actors/alduin/alduin.nif"));

    std::ofstream outFile("C:/Games/Steam/steamapps/common/Skyrim Special Edition/Data/test.nif", std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(test_cont.data()), test_cont.size());
    outFile.close();

    vector<std::byte> test_cont2 = bdi.getFile(fs::path("Scripts/armoraddon.pex"));

    std::ofstream outFile2("C:/Games/Steam/steamapps/common/Skyrim Special Edition/Data/test.pex", std::ios::binary);
    outFile2.write(reinterpret_cast<const char*>(test_cont2.data()), test_cont2.size());
    outFile2.close();
    */

    // Close Console
	exitWithUserInput(0);
}
