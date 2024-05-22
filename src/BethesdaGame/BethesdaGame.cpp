#include <windows.h>

#include "BethesdaGame.h"

using namespace std;
namespace fs = std::filesystem;

BethesdaGame::BethesdaGame(GameType game_type, string game_path) {
	// game_type == SKYRIM_SE: Skyrim Special Edition
	// game_type == SKYRIM_VR: Skyrim VR
	// game_type == SKYRIM: Skyrim
	// game_type == FO4: Fallout 4
	// game_type == FO4_VR: Fallout 4 VR
	this->game_type = game_type;

	if (game_path.empty()) {
		// If the game path is empty, find
		this->game_path = this->findGamePathFromSteam();
	}
	else {
		// If the game path is not empty, use the provided game path
		this->game_path = game_path;
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

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, reg_path.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		// Query the value
		if (RegQueryValueExA(hKey, "InstallLocation", nullptr, &dataType, (LPBYTE)data, &dataSize) == ERROR_SUCCESS) {
			// Ensure null-terminated string
			data[dataSize] = '\0';
			return string(data);
		}

		// Close the registry key
		RegCloseKey(hKey);
	}

	return "";
}

int BethesdaGame::getSteamGameID() const {
	return BethesdaGame::steam_game_ids.at(this->game_type);
}
