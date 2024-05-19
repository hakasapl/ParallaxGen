#ifndef BETHESDADIRECTORYITERATOR_H
#define BETHESDADIRECTORYITERATOR_H

#include <string>
#include <filesystem>
#include <unordered_map>
#include <windows.h>
#include <boost/property_tree/ptree.hpp>
#include <array>

#include "BethesdaGame.h"

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
	BethesdaDirectoryIterator(const std::string path, BethesdaGame::GameType game_type);

private:
	void populateFileMap();

	// BSA list building functions
	std::vector<std::string> buildBSAList();
	std::vector<std::string> buildESPPriorityList();
	std::vector<std::string> getBSAFilesFromINIs();
	bool readINIFile(boost::property_tree::ptree& pt, const std::filesystem::path& ini_path);
	static void findBSAFiles(std::vector<std::string>& bsa_list, const std::filesystem::path& directory, const std::string& prefix);

	// Folder finding methods
	std::filesystem::path getGameDocumentPath();
	std::filesystem::path getGameAppdataPath();
	static std::filesystem::path getSystemPath(const GUID& folder_id);
};

#endif
