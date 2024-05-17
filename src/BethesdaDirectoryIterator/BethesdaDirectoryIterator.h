#include <string>
#include <filesystem>
#include <unordered_map>
#include "BethesdaGame.h"

class BethesdaDirectoryIterator {
private:
	std::unordered_map<std::filesystem::path, std::string> fileMap;
	std::filesystem::path data_dir;
	BethesdaGame::GameType game_type;

	std::unordered_map<BethesdaGame::GameType, std::string> gamePathNames = {
		{BethesdaGame::GameType::SKYRIM_SE, "Skyrim Special Edition"},
		{BethesdaGame::GameType::SKYRIM_VR, "Skyrim VR"},
		{BethesdaGame::GameType::SKYRIM, "Skyrim"},
		{BethesdaGame::GameType::FO4, "Fallout4"},
		{BethesdaGame::GameType::FO4_VR, "Fallout4 VR"}
	};

	std::unordered_map<BethesdaGame::GameType, std::string> gameININames = {
		{BethesdaGame::GameType::SKYRIM_SE, "skyrim.ini"},
		{BethesdaGame::GameType::SKYRIM_VR, "skyrim.ini"},
		{BethesdaGame::GameType::SKYRIM, "skyrim.ini"},
		{BethesdaGame::GameType::FO4, "fallout4.ini"},
		{BethesdaGame::GameType::FO4_VR, "fallout4.ini"}
	};

public:
	BethesdaDirectoryIterator(string path = "");

private:
	void populateFileMap();
	vector<std::filesystem::path> buildBSAList(vector<std::string> esp_priority_list);
	vector<std::string> buildESPPriorityList();
	std::string getSkyrimAppdataPath();
};
