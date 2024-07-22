#include "ParallaxGenPlugin/ParallaxGenPlugin.hpp"

#include <windows.h>
#include <spdlog/spdlog.h>
#include <cwchar>
#include <boost/algorithm/string.hpp>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;

ParallaxGenPlugin::ParallaxGenPlugin(ParallaxGenDirectory* pgd)
{
    // constructor
    this->pgd = pgd;

    // INIT xeditlib
    xelib.InitXEdit();

    // Set gamemode to skyrim se
    xelib.SetGameMode(4);

    wchar_t game_path[] = L"C:\\Games\\Steam\\steamapps\\common\\Skyrim Special Edition";
    wchar_t* game_path_ptr = game_path;
    xelib.SetGamePath(game_path_ptr);

    // Load plugins
    int* len = new int;
    xelib.GetActivePlugins(len);
    wstring active_plugins_str = xelib.getResultStringSafe(*len);
    wchar_t* active_plugins = xelib.WStringToWChar(active_plugins_str);

    xelib.LoadPlugins(active_plugins, false);

    unsigned char* status = new unsigned char;
    while (*status != 2)
    {
        xelib.GetLoaderStatus(status);
        spdlog::info("Loader status: {}", *status);

        // sleep for 1 second
        Sleep(1000);
    }

    xelib.BuildReferences(0, true);

    buildNifMap();

    wstring search_query = L"TXST";
    bool result = xelib.GetRecords(0, xelib.WStringToWChar(search_query), true, len);
    unsigned int* result_array = new unsigned int[*len];
    result = xelib.GetResultArray(result_array, *len);

    // create new file
    unsigned int* patch_handler = new unsigned int;
    result = xelib.AddFile(xelib.WStringToWChar(L"test-xelib.esp"), patch_handler);
    result = xelib.AddMaster(*patch_handler, xelib.WStringToWChar(L"Dragonborn.esm"));
    result = xelib.AddMaster(*patch_handler, xelib.WStringToWChar(L"Skyrim.esm"));

    int* len_tex = new int;
    vector<wstring> txst_records;
    for (int i = 0; i < *len; i++)
    {
        unsigned int handler = result_array[i];
        bool* has_element = new bool;
        result = xelib.HasElement(handler, xelib.WStringToWChar(L"Textures (RGB/A)\\[0]"), has_element);
        result = xelib.GetValue(handler, xelib.WStringToWChar(L"Textures (RGB/A)\\[0]"), len_tex);
        wstring record = xelib.getResultStringSafe(*len_tex);
        txst_records.push_back(record);
        
        int* path_len = new int;
        result = xelib.Path(handler, false, false, path_len);
        wstring path = xelib.getResultStringSafe(*path_len);

        unsigned int* old_handler = new unsigned int;
        result = xelib.GetElement(0, xelib.WStringToWChar(path), old_handler);

        unsigned int* result_handler = new unsigned int;
        result = xelib.CopyElement(*old_handler, *patch_handler, false, result_handler);
    }

    
    result = xelib.SaveFile(*patch_handler, xelib.WStringToWChar(L"C:\\Games\\Steam\\steamapps\\common\\Skyrim Special Edition\\Data\\test-xelib.esp"));

    ParallaxGenUtil::exitWithUserInput(1);
}

void ParallaxGenPlugin::buildNifMap()
{
    // Store error results
    bool result;

    // Find all MODL records
    int* len = new int;
    wstring search_query = L"";
    result = xelib.GetRecords(0, xelib.WStringToWChar(search_query), true, len);
}