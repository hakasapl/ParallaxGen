#ifndef BETHESDADIRECTORY_H
#define BETHESDADIRECTORY_H

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

class BethesdaDirectory {
public:
	// struct to store BSA file information
	struct BSAFile {
		std::filesystem::path path;
		bsa::tes4::version version;
		bsa::tes4::archive archive;

		bool operator<(const BSAFile& other) const {
			return path < other.path;
		}
	};

	// comparator for BSA file in case a map is defined with keys as BSAFile
	struct BSAFileComparator {
		bool operator()(const std::shared_ptr<BSAFile>& lhs, const std::shared_ptr<BSAFile>& rhs) const {
			return *lhs < *rhs;
		}
	};

protected:
	// stores the data directory
	std::filesystem::path data_dir;

private:
	// stores the file map
	std::map<std::filesystem::path, std::shared_ptr<BSAFile>> fileMap;

	// stores whether logging is enabled
	bool logging;

	// stores the game type
	BethesdaGame::GameType game_type;

	// these fields will be searched in ini files for manually specified BSA loading
	static inline const std::array<std::string, 3> ini_bsa_fields = {
		"sResourceArchiveList",
		"sResourceArchiveList2",
		"sResourceArchiveListBeta"
	};

	// any file that ends with these strings will be ignored
	// allowed BSAs etc. to be hidden from the file map since this object is an abstraction
	// of the data directory that no longer factors BSAs for downstream users
	std::vector<std::string> extension_blocklist = {
		".bsa",
		".esp",
		".esl",
		".esm"
	};

public:
	// constructor
	BethesdaDirectory(BethesdaGame& bg, const bool logging);

	// File map functions
	void populateFileMap();
	std::map<std::filesystem::path, std::shared_ptr<BSAFile>> getFileMap() const;

	// File functions
	std::vector<std::byte> getFile(const std::filesystem::path rel_path) const;
	std::vector<std::filesystem::path> findFilesBySuffix(const std::string_view suffix, const std::vector<std::string>& parent_blocklist = std::vector<std::string>()) const;

	// BSA functions
	std::vector<std::string> getBSAPriorityList() const;
	std::vector<std::string> getPluginLoadOrder(const bool trim_extension = false) const;

private:
	// Adds BSA files to the file map
	void addBSAFilesToMap();
	// Adds loose files to the file map
	void addLooseFilesToMap();
	// Adds a BSA file to the file map
	void addBSAToFileMap(const std::string& bsa_name);
	// checks if a file is allowed based on blocklist
	bool file_allowed(const std::filesystem::path file_path) const;
	// gets BSA load order from INI files
	std::vector<std::string> getBSAFilesFromINIs() const;
	// gets BSA files in the data directory
	std::vector<std::string> getBSAFilesInDirectory() const;
	// gets BSA files from a given plugin
	std::vector<std::string> findBSAFilesFromPluginName(const std::vector<std::string>& bsa_file_list, const std::string& plugin_prefix) const;
	// gets INI properties from documents
	boost::property_tree::ptree getINIProperties() const;

	// Folder finding methods
	std::filesystem::path getGameDocumentPath() const;
	std::filesystem::path getGameAppdataPath() const;
};

#endif
