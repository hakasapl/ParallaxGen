#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <iostream>

#include "spdlog/spdlog.h"
#include "BethesdaDirectoryIterator.h"
#include "BethesdaGame.h"

using namespace std;
namespace std::filesystem = fs;

BethesdaDirectoryIterator::BethesdaDirectoryIterator(string path, BethesdaGame::GameType game_type)
{
	// Log starting message
	spdlog::info("Opening Data Folder \"{}\"", path);

	// Assign instance vars
	this->data_dir = fs::path(path);
	this->game_type = game_type;

	// Populate map for iterator
	populateFileMap();
}

void BethesdaDirectoryIterator::populateFileMap()
{

}

vector<fs::path> BethesdaDirectoryIterator::buildBSAList(vector<std::string> esp_priority_list)
{
	// get esp priority list
	vector<string> esp_priority_list = buildESPPriorityList();

	vector<fs::path> bsa_list;

	//loop through each esp in the priority list
	for (string esp : esp_priority_list) {
		//check if the esp has a bsa file
		fs::path bsa_path = this->data_dir / (esp + ".bsa");
		if (fs::exists(bsa_path)) {
			//add the bsa to the list
			bsa_list.push_back(bsa_path);
		}
	}
}

vector<std::string> BethesdaDirectoryIterator::buildESPPriorityList()
{
	std::vector<std::string> output_lo;
	fs::path lo_file = getGameAppdataPath() / "loadorder.txt";

	std::ifstream file(lo_file);

	if (!file.is_open()) {
		spdlog::critical("Unable to open loadorder.txt from {}. Exiting.", lo_file);
		exit(1);
	}

	std::string line;
	while (std::getline(file, line)) {
		// Ignore lines that start with '#', which are comment lines
		if (line.empty() || line[0] == '#') {
			continue;
		}

		// Remove extension from line
		line = line.substr(0, line.find_last_of('.'));

		// Add to output vector
		output_lo.push_back(line);
	}

	file.close();

	return output_lo;
}

vector<std::string> BethesdaDirectoryIterator::getBSAFilesFromINI()
{
	vector<string> bsa_files;
	fs::path ini_path = getGameDocumentPath() / this->gameININames[this->game_type];

	std::ifstream file(ini_path);

	if (!file.is_open()) {
		spdlog::critical("Unable to open ini file from {}. Exiting.", ini_path);
		exit(1);
	}


}

fs::path BethesdaDirectoryIterator::getGameDocumentPath()
{
	PWSTR path = NULL;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path);
	if (SUCCEEDED(result)) {
		std::wstring documentsPath(path);
		CoTaskMemFree(path);  // Free the memory allocated for the path

		out_path = fs::path(path);
		out_path /= this->gamePathNames[this->game_type];
		return out_path;
	}
	else {
		// Handle error
		spdlog::critical("Unable to locate documents folder. Exiting.");
		exit(1);
	}
}

fs::path BethesdaDirectoryIterator::getGameAppdataPath()
{
	PWSTR path = NULL;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path);
	if (SUCCEEDED(result)) {
		std::wstring appDataPath(path);
		CoTaskMemFree(path);  // Free the memory allocated for the path

		out_path = fs::path(path);
		out_path /= this->gamePathNames[this->game_type];
		return out_path;
	}
	else {
		// Handle error
		spdlog::critical("Unable to locate local appdata folder. Exiting.");
		exit(1);
	}
}
