#ifndef BETHESDADIRECTORYITERATOR_H
#define BETHESDADIRECTORYITERATOR_H

#include <span>
#include <cstddef>
#include <memory>
#include <string>
#include <filesystem>
#include <map>
#include <array>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <bsa/tes4.hpp>

#include "BethesdaGame/BethesdaGame.hpp"

class BethesdaDirectoryIterator {
public:
	struct BSAFile {
		std::filesystem::path path;
		bsa::tes4::version version;
		bsa::tes4::archive archive;
	};

private:
	std::map<std::filesystem::path, std::shared_ptr<BSAFile>> fileMap;
	std::filesystem::path data_dir;
	BethesdaGame::GameType game_type;

	static inline const std::array<std::string, 3> ini_bsa_fields = {
		"sResourceArchiveList",
		"sResourceArchiveList2",
		"sResourceArchiveListBeta"
	};

public:
	BethesdaDirectoryIterator(BethesdaGame bg);

	void populateFileMap();
	std::map<std::filesystem::path, std::shared_ptr<BSAFile>> getFileMap() const;
	std::vector<std::byte> getFile(std::filesystem::path rel_path) const;

	// BSA functions
	std::vector<std::string> getBSAPriorityList() const;
	std::vector<std::string> getPluginLoadOrder(bool trim_extension) const;

private:
	// BSA list building functions
	void addLooseFilesToMap();
	void addBSAToFileMap(const std::string& bsa_name);
	std::vector<std::string> getBSAFilesFromINIs() const;
	std::vector<std::string> getBSAFilesInDirectory() const;
	std::vector<std::string> findBSAFilesFromPluginName(const std::vector<std::string>& bsa_file_list, const std::string& plugin_prefix) const;
	boost::property_tree::ptree getINIProperties() const;

	// Folder finding methods
	std::filesystem::path getGameDocumentPath() const;
	std::filesystem::path getGameAppdataPath() const;
};

#endif
