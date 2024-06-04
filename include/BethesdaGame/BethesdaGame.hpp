#ifndef BETHESDAGAME_H
#define BETHESDAGAME_H

#include <unordered_map>
#include <filesystem>
#include <string>

class BethesdaGame {
public:
	// GameType enum
	enum class GameType {
		SKYRIM_SE,
		SKYRIM_VR,
		SKYRIM,
		ENDERAL,
		ENDERAL_SE
	};

	// StoreType enum (for now only Steam is used)
	enum class StoreType {
		STEAM,
		WINDOWS_STORE,
		EPIC_GAMES_STORE,
		GOG
	};

	// struct that stores location of ini and custom ini file for a game
	struct ININame {
		std::filesystem::path ini;
		std::filesystem::path ini_custom;
	};

	// Define INI locations for each game
	static inline const std::unordered_map<BethesdaGame::GameType, ININame> INILocations = {
		{BethesdaGame::GameType::SKYRIM_SE, ININame{"skyrim.ini", "skyrimcustom.ini"}},
		{BethesdaGame::GameType::SKYRIM_VR, ININame{"skyrim.ini", "skyrimcustom.ini"}},
		{BethesdaGame::GameType::SKYRIM, ININame{"skyrim.ini", "skyrimcustom.ini"}},
		{BethesdaGame::GameType::ENDERAL, ININame{"enderal.ini", "enderalcustom.ini"}},
		{BethesdaGame::GameType::ENDERAL_SE, ININame{"enderal.ini", "enderalcustom.ini"}}
	};

	// Define document folder location for each game
	static inline const std::unordered_map<BethesdaGame::GameType, std::filesystem::path> DocumentLocations = {
		{BethesdaGame::GameType::SKYRIM_SE, "My Games/Skyrim Special Edition"},
		{BethesdaGame::GameType::SKYRIM_VR, "My Games/Skyrim VR"},
		{BethesdaGame::GameType::SKYRIM, "My Games/Skyrim"},
		{BethesdaGame::GameType::ENDERAL, "My Games/Enderal"},
		{BethesdaGame::GameType::ENDERAL_SE, "My Games/Enderal Special Edition"}
	};

	// Define appdata folder location for each game
	static inline const std::unordered_map<BethesdaGame::GameType, std::filesystem::path> AppDataLocations = {
		{BethesdaGame::GameType::SKYRIM_SE, "Skyrim Special Edition"},
		{BethesdaGame::GameType::SKYRIM_VR, "Skyrim VR"},
		{BethesdaGame::GameType::SKYRIM, "Skyrim"},
		{BethesdaGame::GameType::ENDERAL, "Enderal"},
		{BethesdaGame::GameType::ENDERAL_SE, "Enderal Special Edition"}
	};

private:
	// stores the game type
	GameType game_type;

	// stores game path and game data path (game path / data)
	std::filesystem::path game_path;
	std::filesystem::path game_data_path;

	// stores whether logging is enabled
	bool logging;

	// stores the steam game ids for each game
	inline static const std::unordered_map<BethesdaGame::GameType, int> steam_game_ids = {
		{BethesdaGame::GameType::SKYRIM_SE, 489830},
		{BethesdaGame::GameType::SKYRIM_VR, 611670},
		{BethesdaGame::GameType::SKYRIM, 72850},
		{BethesdaGame::GameType::ENDERAL, 933480},
		{BethesdaGame::GameType::ENDERAL_SE, 976620}
	};

	inline static const std::string lookup_file = "Skyrim.esm";

public:
	// constructor
	BethesdaGame(GameType game_type, const std::filesystem::path game_path, const bool logging = false);

	// get functions
	GameType getGameType() const;
	std::filesystem::path getGamePath() const;
	std::filesystem::path getGameDataPath() const;

private:
	// locates the steam install locatino of steam
	std::filesystem::path findGamePathFromSteam() const;
	// returns the steam ID of the current game
	int getSteamGameID() const;
};

#endif
