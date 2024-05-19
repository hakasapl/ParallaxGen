#include <iostream>
#include <fstream>
#include <string>
#include <bsa/tes4.hpp>
#include <cstdio>
#include <filesystem>
#include <tuple>

#include "CLI/CLI.hpp"
#include "spdlog/spdlog.h"

using namespace std;
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



    // Close Console
    cout << "Press ENTER to close...";
    cin.get();
}
