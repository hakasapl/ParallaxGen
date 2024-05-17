#ifndef BETHESDAGAME_H
#define BETHESDAGAME_H

#include <unordered_map>
#include <filesystem>
#include <string>

class BethesdaGame {
public:
	enum GameType {
		SKYRIM_SE,
		SKYRIM_VR,
		SKYRIM,
		FO4,
		FO4_VR
	};

	enum StoreType {
		STEAM,
		WINDOWS_STORE,
		EPIC_GAMES_STORE,
		GOG
	};

private:
	GameType game_type;
	std::filesystem::path game_path;

	inline static const std::unordered_map<BethesdaGame::GameType, int> steam_game_ids = {
		{SKYRIM_SE, 489830},
		{SKYRIM_VR, 611670},
		{SKYRIM, 72850},
		{FO4, 377160},
		{FO4_VR, 611880}
	};

public:
	BethesdaGame(GameType game_type, std::string game_path = "");

	GameType getGameType() const;
	std::filesystem::path getGamePath() const;
	std::filesystem::path findGamePathFromSteam() const;

private:
	int getSteamGameID() const;
};

#endif
