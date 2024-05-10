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

    // Create and verify paths
    fs::path datapath{ "C:/Games/Steam/steamapps/common/Skyrim Special Edition/Data" };  // Game path
    if (!fs::exists(datapath) || !fs::is_directory(datapath)) {
        spdlog::critical("Game data folder is not found or is not a directory");
        return 1;
    }

    fs::path loadorderpath{ "C:/Users/Hakan/AppData/Local/Skyrim Special Edition/loadorder.txt" };  // Load order file path
    if (!fs::exists(loadorderpath) || !fs::is_regular_file(loadorderpath)) {
        spdlog::critical("Load order folder not found. If you are a Mod Organizer user, this program must be run from within MO2");
        return 1;
    }

    // Import load order into vector
    ifstream lo_filehandle(loadorderpath);
    vector<string> lo_lines;
    string cur_line;

    // check if file exists and can be opened
    spdlog::info("Reading load order...");

    if (!lo_filehandle) {
        spdlog::critical("Error opening load order file");
        return 1;
    }

    while (getline(lo_filehandle, cur_line)) {
        if (!cur_line.empty() && cur_line[0] != '#') {
            lo_lines.push_back(cur_line);

            if (debug_mode) {
                spdlog::debug(cur_line);
            }
        }
    }

    lo_filehandle.close();

    spdlog::info("Found {} plugins", size(lo_lines));

    // Find BSAs
    spdlog::info("Searching for BSA files...");

    vector<string> bsa_files;

    for (const auto& entry : fs::directory_iterator(datapath)) {
        if (entry.is_regular_file()) {
            if (entry.path().extension() == ".bsa") {
                // Found BSA file
                string cur_filename = entry.path().filename().string();
                bsa_files.push_back(cur_filename);

                if (debug_mode) {
                    spdlog::debug("Found BSA: {}", cur_filename);
                }
            }
        }
    }

    spdlog::info("Found {} BSA files", size(bsa_files));

    unordered_map<string, string> nif_list; // List of NIFs <path to file, bsa file (if exists)
    unordered_map<string, string> dds_p_list;  // List of height maps
    unordered_map<string, string> dds_m_list;  // List of diffuse textures
    vector<tuple<string, int, int>> nif_proc_list;  // Mesh filename, object int (do we also need to store string?), mesh mode to set (0 = no change, 1 = parallax mesh, 2 = complex parallax mesh)

    // LOOKUP STEP
    // Loop through each esp in load order
    //     Loop through each bsa that is part of esp
    //         Add each nif file found to nif_list
    //         Add each dds_p file found to dds_p_list
    //         Add each dds_m env mask file found to dds_list
    //             Only add if _m file has an alpha layer that isn't 255
    // Recursively loop through each file in data folder
    //     Add each nif file found to nif_list
    //     Add each dds_p file found to dds_p_list
    //     Add each dds diffuse file found to dds_list
    //         Only add if _m file has an alpha layer that isn't 255
    // PROCESSING STEP
    // Loop through nif files in nif_list
    //     Loop through each mesh in nif file, gather texture maps
    //         Does a _p map already exist?
    //             Verify that the mesh is set correctly, then move on. If it's not set correctly, set mode to 1
    //         Does a _m map already exist?
    //             Verify that the mesh is set correctly, then move on. If it's not set correctly, set mode to 2
    //         Check if texture map _p file exists in dds_p_list. If yes, add to nif_proc_list with mode 1
    //         Check if texture map _m map exists in dds_m_list. If yes, add to nif_proc_list with mode 2
    // Loop through nif_list
    //     Loop through each nif and process parallax meshes or complex parallax meshes for each item

    // Close Console
    cout << "Press ENTER to close...";
    cin.get();
}
