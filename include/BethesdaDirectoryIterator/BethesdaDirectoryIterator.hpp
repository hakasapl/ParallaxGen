#ifndef BETHESDADIRECTORYITERATOR_H
#define BETHESDADIRECTORYITERATOR_H

#include <string>
#include <filesystem>
#include <map>
#include <array>
#include <tuple>
#include <boost/property_tree/ptree.hpp>

#include <bsa/tes4.hpp>

#include "BethesdaGame/BethesdaGame.hpp"

class BethesdaDirectoryIterator {
private:
	std::map<std::filesystem::path, std::string> fileMap;
	std::filesystem::path data_dir;
	BethesdaGame::GameType game_type;

public:
	static inline const std::unordered_map<BethesdaGame::GameType, std::string> gamePathNames = {
		{BethesdaGame::GameType::SKYRIM_SE, "Skyrim Special Edition"},
		{BethesdaGame::GameType::SKYRIM_VR, "Skyrim VR"},
		{BethesdaGame::GameType::SKYRIM, "Skyrim"}
	};

	static inline const std::unordered_map<BethesdaGame::GameType, std::string> gameININames = {
		{BethesdaGame::GameType::SKYRIM_SE, "skyrim.ini"},
		{BethesdaGame::GameType::SKYRIM_VR, "skyrim.ini"},
		{BethesdaGame::GameType::SKYRIM, "skyrim.ini"}
	};

	static inline const std::unordered_map<BethesdaGame::GameType, std::string> gameINICustomNames = {
		{BethesdaGame::GameType::SKYRIM_SE, "skyrimcustom.ini"},
		{BethesdaGame::GameType::SKYRIM_VR, "skyrimcustom.ini"},
		{BethesdaGame::GameType::SKYRIM, "skyrimcustom.ini"}
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
	std::vector<std::string> getBSAPriorityList() const;
	std::vector<std::string> getPluginLoadOrder(bool trim_extension) const;

private:
	// BSA list building functions
	void addLooseFilesToMap();
	void addBSAToFileMap(const std::string& bsa_name, bsa::tes4::archive& bsa_obj);
	std::vector<std::string> getBSAFilesFromINIs() const;
	std::vector<std::string> getBSAFilesInDirectory() const;
	std::vector<std::string> findBSAFilesFromPluginName(const std::vector<std::string>& bsa_file_list, const std::string& plugin_prefix) const;
	boost::property_tree::ptree getINIProperties() const;

	// Folder finding methods
	std::filesystem::path getGameDocumentPath() const;
	std::filesystem::path getGameAppdataPath() const;
};

#endif
