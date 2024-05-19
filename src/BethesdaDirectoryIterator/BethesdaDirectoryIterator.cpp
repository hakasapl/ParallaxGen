#include "BethesdaDirectoryIterator.h"

#include <knownfolders.h>
#include <shlobj.h>
#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>

#include "spdlog/spdlog.h"

using namespace std;
namespace fs = filesystem;

template <typename T>
static void appendToVectorExclusively(vector<T>& vec, const T item) {
	if (find(vec.begin(), vec.end(), item) == vec.end()) {
		vec.push_back(item);
	}
}

BethesdaDirectoryIterator::BethesdaDirectoryIterator(const string path, BethesdaGame::GameType game_type)
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

vector<string> BethesdaDirectoryIterator::buildBSAList()
{
	// get bsa files not loaded from esp (also initializes output vector)
	vector<string> bsa_files = getBSAFilesFromINIs();

	// get esp priority list
	const vector<string> esp_priority_list = buildESPPriorityList();

	//loop through each esp in the priority list
	for (string esp : esp_priority_list) {
		// add any BSAs to list
		BethesdaDirectoryIterator::findBSAFiles(bsa_files, this->data_dir, esp);
	}

	return bsa_files;
}

vector<string> BethesdaDirectoryIterator::buildESPPriorityList()
{
	vector<string> output_lo;
	fs::path lo_file = getGameAppdataPath() / "loadorder.txt";

	ifstream file(lo_file);

	if (!file.is_open()) {
		spdlog::critical("Unable to open loadorder.txt from {}. Exiting.", lo_file.string());
		exit(1);
	}

	string line;
	while (getline(file, line)) {
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

vector<string> BethesdaDirectoryIterator::getBSAFilesFromINIs()
{
	vector<string> bsa_files;

	// get document path
	fs::path doc_path = getGameDocumentPath();

	// get ini file paths
	fs::path skyrim_ini_path = doc_path / BethesdaDirectoryIterator::gameININames.at(this->game_type);
	fs::path skyrim_custom_ini_path = doc_path / BethesdaDirectoryIterator::gameINICustomNames.at(this->game_type);

	boost::property_tree::ptree pt_skyrim_ini;
	boost::property_tree::ptree pt_skyrim_custom_ini;

	// attempt to read INI files
	if (!this->readINIFile(pt_skyrim_ini, skyrim_ini_path)) {
		spdlog::critical("Unable to read {}. Exiting.", skyrim_ini_path.string());
		exit(1);
	}

	bool custom_exists = this->readINIFile(pt_skyrim_custom_ini, skyrim_ini_path);
	if (!custom_exists) {
		spdlog::critical("Unable to read {}. Ignoring (not required).", skyrim_custom_ini_path.string());
	}

	// loop through each archive field in the INI files
	for (string field : this->ini_bsa_fields) {
		try {
			string cur_val = pt_skyrim_ini.get<string>("Archive." + field);

			// check if there is an override field in custom.ini
			if (custom_exists) {
				try {
					string custom_val = pt_skyrim_custom_ini.get<string>("Archive." + field);
				}
				catch (const exception& e) {
					spdlog::info("Unable to find {} in [Archive] section of skyrimcustom.ini: {}: Ignoring.", field, e.what());
				}
			}

			// parse string into vector
			stringstream cur_ss(cur_val);
			string token;

			while (getline(cur_ss, token, ',')) {
				token.erase(token.find_last_not_of(" \n\r\t") + 1);
				token.erase(0, token.find_first_not_of(" \n\r\t"));

				appendToVectorExclusively(bsa_files, token);
			}
		}
		catch (const exception& e) {
			spdlog::info("Unable to find {} in [Archive] section of skyrim.ini: {}: Ignoring.", field, e.what());
		}
	}

	return bsa_files;
}

bool BethesdaDirectoryIterator::readINIFile(boost::property_tree::ptree& pt, const fs::path& ini_path)
{
	try {
		ifstream iniFileStream(ini_path);
		if (!iniFileStream) {
			return false;
		}

		read_ini(iniFileStream, pt);

		iniFileStream.close();
		return true;
	}
	catch (const exception& e) {
		spdlog::debug("Error reading {}: {}", ini_path.string(), e.what());
		return false;
	}
}

void BethesdaDirectoryIterator::findBSAFiles(vector<string>& bsa_list, const fs::path& directory, const string& prefix) {
	try {
		for (const auto& entry : fs::directory_iterator(directory)) {
			if (entry.is_regular_file()) {
				const string file_extension = entry.path().extension().string();
				// only interested in BSA files
				if (!boost::iequals(file_extension, ".bsa")) {
					continue;
				}

				// check filename without extension
				string stem = entry.path().stem().string();

				// isolate the part before the - textures etc.
				const size_t pos_space = stem.find(" - ");
				if (pos_space != string::npos) {
					stem = stem.substr(0, pos_space);
				}

				if (stem == prefix) {
					// BSA we are concerned with
					const string filename = entry.path().filename().string();
					appendToVectorExclusively(bsa_list, filename);
				}
			}
		}
	}
	catch (const filesystem::filesystem_error& e) {
		spdlog::critical("Filesystem error: {}", e.what());
		exit(1);
	}
}

fs::path BethesdaDirectoryIterator::getGameDocumentPath()
{
	fs::path doc_path = BethesdaDirectoryIterator::getSystemPath(FOLDERID_Documents);
	doc_path /= BethesdaDirectoryIterator::gamePathNames.at(this->game_type);
	return doc_path;
}

fs::path BethesdaDirectoryIterator::getGameAppdataPath()
{
	fs::path appdata_path = BethesdaDirectoryIterator::getSystemPath(FOLDERID_LocalAppData);
	appdata_path /= BethesdaDirectoryIterator::gamePathNames.at(this->game_type);
	return appdata_path;
}

fs::path BethesdaDirectoryIterator::getSystemPath(const GUID& folder_id)
{
	PWSTR path = NULL;
	HRESULT result = SHGetKnownFolderPath(folder_id, 0, NULL, &path);
	if (SUCCEEDED(result)) {
		wstring appDataPath(path);
		CoTaskMemFree(path);  // Free the memory allocated for the path

		return fs::path(path);
	}
	else {
		// Handle error
		spdlog::critical("Unable to locate system folder. Exiting.");
		exit(1);
	}
}
