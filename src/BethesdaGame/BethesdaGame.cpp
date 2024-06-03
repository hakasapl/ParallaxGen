#include "BethesdaGame/BethesdaGame.hpp"

#include <windows.h>
#include <spdlog/spdlog.h>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
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
			spdlog::critical("Unable to locate game data path. If you are using a non-Steam version of skyrim, please specify the game data path manually using the -d argument.");
			ParallaxGenUtil::exitWithUserInput(1);
		} else {
			throw runtime_error("Game path not found");
		}
	}

	// check if path actually exists
	if (!fs::exists(this->game_path)) {
		// If the game path does not exist, throw an exception
		if (this->logging) {
			spdlog::critical("Game path does not exist: {}", this->game_path.string());
			ParallaxGenUtil::exitWithUserInput(1);
		} else {
			throw runtime_error("Game path does not exist");
		}
	}

	this->game_data_path = this->game_path / "Data";
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
	return BethesdaGame::steam_game_ids.at(this->game_type);
}
