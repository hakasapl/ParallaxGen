#include "BethesdaGame/BethesdaGame.hpp"

#include <spdlog/spdlog.h>
#include <knownfolders.h>
#include <shlobj.h>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
namespace fs = std::filesystem;

BethesdaGame::BethesdaGame(GameType game_type, const fs::path game_path, bool logging) {
	this->logging = logging;

	// game_type == SKYRIM_SE: Skyrim Special Edition
	// game_type == SKYRIM_VR: Skyrim VR
	// game_type == SKYRIM: Skyrim
	this->game_type = game_type;

	if (game_path.empty()) {
		// If the game path is empty, find
		this->game_path = this->findGamePathFromSteam();
	}
	else {
		// If the game path is not empty, use the provided game path
		this->game_path = game_path;
	}

	if (this->game_path.empty()) {
		// If the game path is still empty, throw an exception
		if (this->logging) {
			spdlog::critical("Unable to locate game data path. Please specify the game data path manually using the -d argument.");
			exitWithUserInput(1);
		} else {
			throw runtime_error("Game path not found");
		}
	}

	// check if path actually exists
	if (!fs::exists(this->game_path)) {
		// If the game path does not exist, throw an exception
		if (this->logging) {
			spdlog::critical(L"Game path does not exist: {}", this->game_path.wstring());
			exitWithUserInput(1);
		} else {
			throw runtime_error("Game path does not exist");
		}
	}

	this->game_data_path = this->game_path / "Data";

	if (!fs::exists(this->game_data_path / lookup_file)) {
		// If the game data path does not contain Skyrim.esm, throw an exception
		if (this->logging) {
			spdlog::critical(L"Game data path does not contain Skyrim.esm, which probably means it's invalid: {}", this->game_data_path.wstring());
			exitWithUserInput(1);
		} else {
			throw runtime_error("Game data path does not contain Skyrim.esm");
		}
	}
}

BethesdaGame::GameType BethesdaGame::getGameType() const {
	return this->game_type;
}

fs::path BethesdaGame::getGamePath() const {
	// Get the game path from the registry
	// If the game is not found, return an empty string
	return this->game_path;
}

fs::path BethesdaGame::getGameDataPath() const {
	return this->game_data_path;
}

fs::path BethesdaGame::findGamePathFromSteam() const {
	// Find the game path from the registry
	// If the game is not found, return an empty string

	// Check if key doesn't exists in steam map
	if (BethesdaGame::steam_game_ids.find(game_type) == BethesdaGame::steam_game_ids.end()) {
		return fs::path();
	}

	string reg_path = R"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Steam App )" + to_string(this->getSteamGameID());

	HKEY hKey;
	char data[1024]{};
	DWORD dataType;
	DWORD dataSize = sizeof(data);

	LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, reg_path.c_str(), 0, KEY_READ, &hKey);
	if (result == ERROR_SUCCESS) {
		// Query the value
		result = RegQueryValueExA(hKey, "InstallLocation", nullptr, &dataType, (LPBYTE)data, &dataSize);
		if (result == ERROR_SUCCESS) {
			// Ensure null-terminated string
			data[dataSize] = '\0';
			return string(data);
		}
		// Close the registry key
		RegCloseKey(hKey);
	}

	return fs::path();
}

int BethesdaGame::getSteamGameID() const {
	return BethesdaGame::steam_game_ids.at(game_type);
}

BethesdaGame::ININame BethesdaGame::getINIPaths() const
{
	BethesdaGame::ININame output = INILocations.at(game_type);
	fs::path game_docs_path = getGameDocumentPath();
	
	// normal ini file
	output.ini = game_docs_path / output.ini;
	output.ini_prefs = game_docs_path / output.ini_prefs;
	output.ini_custom = game_docs_path / output.ini_custom;

	return output;
}

fs::path BethesdaGame::getLoadOrderFile() const
{
	fs::path game_appdata_path = getGameAppdataPath();
	fs::path load_order_file = game_appdata_path / "loadorder.txt";
	return load_order_file;
}

fs::path BethesdaGame::getGameDocumentPath() const
{
	fs::path doc_path = getSystemPath(FOLDERID_Documents);
	if (doc_path.empty()) {
		return fs::path();
	}

	doc_path /= BethesdaGame::DocumentLocations.at(game_type);
	return doc_path;
}

fs::path BethesdaGame::getGameAppdataPath() const
{
	fs::path appdata_path = getSystemPath(FOLDERID_LocalAppData);
	if (appdata_path.empty()) {
		return fs::path();
	}

	appdata_path /= BethesdaGame::AppDataLocations.at(game_type);
	return appdata_path;
}

fs::path BethesdaGame::getSystemPath(const GUID& folder_id) const
{
	PWSTR path = NULL;
	HRESULT result = SHGetKnownFolderPath(folder_id, 0, NULL, &path);
	if (SUCCEEDED(result)) {
		wstring appDataPath(path);
		CoTaskMemFree(path);  // Free the memory allocated for the path

		return fs::path(path);
	}
	else {
		// Handle error
		return fs::path();
	}
}
