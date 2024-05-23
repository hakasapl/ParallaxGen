#ifndef BETHESDADIRECTORYITERATOR_H
#define BETHESDADIRECTORYITERATOR_H

#include <string>
#include <filesystem>
#include <unordered_map>
#include <array>
#include <boost/property_tree/ptree.hpp>

#include "BethesdaGame/BethesdaGame.hpp"

class BethesdaDirectoryIterator {
private:
	std::unordered_map<std::filesystem::path, std::string> fileMap;
	std::filesystem::path data_dir;
	BethesdaGame::GameType game_type;

public:
	static inline const std::unordered_map<BethesdaGame::GameType, std::string> gamePathNames = {
		{BethesdaGame::GameType::SKYRIM_SE, "Skyrim Special Edition"},
		{BethesdaGame::GameType::SKYRIM_VR, "Skyrim VR"},
		{BethesdaGame::GameType::SKYRIM, "Skyrim"},
		{BethesdaGame::GameType::FO4, "Fallout4"},
		{BethesdaGame::GameType::FO4_VR, "Fallout4 VR"}
	};

	static inline const std::unordered_map<BethesdaGame::GameType, std::string> gameININames = {
		{BethesdaGame::GameType::SKYRIM_SE, "skyrim.ini"},
		{BethesdaGame::GameType::SKYRIM_VR, "skyrim.ini"},
		{BethesdaGame::GameType::SKYRIM, "skyrim.ini"},
		{BethesdaGame::GameType::FO4, "fallout4.ini"},
		{BethesdaGame::GameType::FO4_VR, "fallout4.ini"}
	};

	static inline const std::unordered_map<BethesdaGame::GameType, std::string> gameINICustomNames = {
		{BethesdaGame::GameType::SKYRIM_SE, "skyrimcustom.ini"},
		{BethesdaGame::GameType::SKYRIM_VR, "skyrimcustom.ini"},
		{BethesdaGame::GameType::SKYRIM, "skyrimcustom.ini"},
		{BethesdaGame::GameType::FO4, "fallout4custom.ini"},
		{BethesdaGame::GameType::FO4_VR, "fallout4custom.ini"}
	};

	static inline const std::array<std::string, 3> ini_bsa_fields = {
		"sResourceArchiveList",
		"sResourceArchiveList2",
		"sResourceArchiveListBeta"
	};

public:
	BethesdaDirectoryIterator(BethesdaGame bg);

	void populateFileMap();

	// BSA functions
	std::vector<std::string> getBSAPriorityList();
	std::vector<std::string> getPluginLoadOrder(bool trim_extension);

private:
	// BSA list building functions
	std::vector<std::string> getBSAFilesFromINIs();
	std::vector<std::string> getBSAFilesInDirectory();
	std::vector<std::string> findBSAFilesFromPluginName(const std::vector<std::string>& bsa_file_list, const std::string& plugin_prefix);
	boost::property_tree::ptree getINIProperties();

	// Folder finding methods
	std::filesystem::path getGameDocumentPath();
	std::filesystem::path getGameAppdataPath();
};

#endif
