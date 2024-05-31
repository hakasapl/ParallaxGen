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
	struct BSAFile {
		std::filesystem::path path;
		bsa::tes4::version version;
		bsa::tes4::archive archive;

		bool operator<(const BSAFile& other) const {
			return path < other.path;
		}
	};

	struct BSAFileComparator {
		bool operator()(const std::shared_ptr<BSAFile>& lhs, const std::shared_ptr<BSAFile>& rhs) const {
			return *lhs < *rhs;
		}
	};

protected:
	std::filesystem::path data_dir;

private:
	std::map<std::filesystem::path, std::shared_ptr<BSAFile>> fileMap;
	bool logging;
	BethesdaGame::GameType game_type;

	static inline const std::array<std::string, 3> ini_bsa_fields = {
		"sResourceArchiveList",
		"sResourceArchiveList2",
		"sResourceArchiveListBeta"
	};

	std::vector<std::string> extension_blocklist = {
		".bsa",
		".esp",
		".esl",
		".esm"
	};

public:
	BethesdaDirectory(BethesdaGame bg, bool logging);

	// File map functions
	void populateFileMap();
	std::map<std::filesystem::path, std::shared_ptr<BSAFile>> getFileMap() const;

	// File functions
	std::vector<std::byte> getFile(std::filesystem::path rel_path) const;
	std::vector<std::filesystem::path> findFilesBySuffix(std::string_view suffix, std::vector<std::string> parent_blocklist = std::vector<std::string>()) const;
	std::map<std::shared_ptr<BSAFile>, std::filesystem::path, BSAFileComparator> findFilesBySuffixKeyedContainer(std::string_view suffix) const;
	std::map<std::filesystem::path, std::shared_ptr<BSAFile>> findFilesBySuffixKeyedFile(std::string_view suffix) const;

	// BSA functions
	std::vector<std::string> getBSAPriorityList() const;
	std::vector<std::string> getPluginLoadOrder(bool trim_extension) const;

private:
	// BSA list building functions
	void addBSAFilesToMap();
	void addLooseFilesToMap();
	void addBSAToFileMap(const std::string& bsa_name);
	bool file_allowed(const std::filesystem::path file_path) const;
	std::vector<std::string> getBSAFilesFromINIs() const;
	std::vector<std::string> getBSAFilesInDirectory() const;
	std::vector<std::string> findBSAFilesFromPluginName(const std::vector<std::string>& bsa_file_list, const std::string& plugin_prefix) const;
	boost::property_tree::ptree getINIProperties() const;

	// Folder finding methods
	std::filesystem::path getGameDocumentPath() const;
	std::filesystem::path getGameAppdataPath() const;
};

#endif
