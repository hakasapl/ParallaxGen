//todo: Figure out includes
//todo: Add more debug logging

#include "BethesdaDirectory/BethesdaDirectory.hpp"

#include <windows.h>
#include <knownfolders.h>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <binary_io/binary_io.hpp>

// BSA Includes
#include <cstdio>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
namespace fs = filesystem;

BethesdaDirectory::BethesdaDirectory(BethesdaGame bg, bool logging)
{
	this->logging = logging;

	if(this->logging) {
		// Log starting message
		spdlog::info("Opening Data Folder \"{}\"", bg.getGameDataPath().string());
	}

	// Assign instance vars
	data_dir = fs::path(bg.getGameDataPath());
	game_type = bg.getGameType();

	// Populate map for iterator
	populateFileMap();
}

void BethesdaDirectory::populateFileMap()
{
	// clear map before populating
	fileMap.clear();

	// add BSA files to file map
	addBSAFilesToMap();

	// add loose files to file map
	addLooseFilesToMap();
}

map<fs::path, shared_ptr<BethesdaDirectory::BSAFile>> BethesdaDirectory::getFileMap() const
{
	return fileMap;
}

vector<std::byte> BethesdaDirectory::getFile(fs::path rel_path) const
{
	// find bsa/loose file to open
	shared_ptr<BSAFile> bsa_struct = fileMap.at(rel_path);
	if (bsa_struct == nullptr) {
		return vector<std::byte>();
		// this is a loose file
		fs::path file_path = data_dir / rel_path;
		binary_io::file_istream fis(file_path);

		// create buffer to store bytes
		//todo: is there a way to avoid the span here?
		vector<std::byte> buffer(fs::file_size(file_path));
		span<std::byte> span(buffer);
		fis.read_bytes(span);

		return buffer;
	}
	else {
		// this is a bsa archive file
		bsa::tes4::version bsa_version = bsa_struct->version;
		bsa::tes4::archive bsa_obj = bsa_struct->archive;
		const auto file = bsa_obj[rel_path.parent_path().string()][rel_path.filename().string()];
		if (file) {
			binary_io::any_ostream aos{ std::in_place_type<binary_io::memory_ostream> };
			// read file from output stream
			file->write(aos, bsa_version);

			auto& s = aos.get<binary_io::memory_ostream>();
			return s.rdbuf();
		}
		else {
			throw runtime_error("File not found in BSA archive. This can only happen if the file was deleted after this object was constructed.");
		}
	}
}

//todo: reduce these repeated methods
vector<fs::path> BethesdaDirectory::findFilesBySuffix(string_view suffix, vector<string> path_blocklist) const
{
	// find all keys in fileMap that match pattern
	vector<fs::path> found_files;

	// loop through filemap and match keys
	for (const auto& [key, value] : fileMap) {
        if (boost::algorithm::ends_with(key.wstring(), suffix)) {
			// check if any component of the path is in the blocklist
			bool block = false;
			for (string block_path : path_blocklist) {
				if (boost::algorithm::contains(key.wstring(), block_path)) {
					block = true;
					break;
				}
			}

			if (block) {
				continue;
			}

			found_files.push_back(key);
		}
	}

	return found_files;
}

map<shared_ptr<BethesdaDirectory::BSAFile>, fs::path, BethesdaDirectory::BSAFileComparator> BethesdaDirectory::findFilesBySuffixKeyedContainer(std::string_view suffix) const
{
	map<shared_ptr<BethesdaDirectory::BSAFile>, fs::path, BethesdaDirectory::BSAFileComparator> found_files;

	for (const auto& [key, value] : fileMap) {
		if (boost::algorithm::ends_with(key.wstring(), suffix)) {
			found_files[value] = key;
		}
	}

	return found_files;
}

map<fs::path, shared_ptr<BethesdaDirectory::BSAFile>> BethesdaDirectory::findFilesBySuffixKeyedFile(std::string_view suffix) const
{
	map<fs::path, shared_ptr<BethesdaDirectory::BSAFile>> found_files;

	for (const auto& [key, value] : fileMap) {
		if (boost::algorithm::ends_with(key.wstring(), suffix)) {
			found_files[key] = value;
		}
	}

	return found_files;
}

void BethesdaDirectory::addBSAFilesToMap()
{
	if(logging) {
		spdlog::info("Adding BSA files to file map.");
	}

	// Get list of BSA files
	vector<string> bsa_files = getBSAPriorityList();

	// Loop through each BSA file
	for (string bsa_name : bsa_files) {
		// add bsa to file map
		addBSAToFileMap(bsa_name);
	}
}

void BethesdaDirectory::addLooseFilesToMap()
{
	if(logging) {
		spdlog::info("Adding loose files to file map.");
	}

	for (const auto& entry : fs::recursive_directory_iterator(data_dir)) {
		if (entry.is_regular_file()) {
			const fs::path file_path = entry.path();
			const fs::path relative_path = file_path.lexically_relative(data_dir);

			// check type of file, skip BSAs and ESPs
			if (!file_allowed(file_path)) {
				continue;
			}

			fileMap[relative_path] = nullptr;
		}
	}
}

void BethesdaDirectory::addBSAToFileMap(const string& bsa_name)
{
	if(logging) {
		// log message
		spdlog::debug("Adding files from {} to file map.", bsa_name);
	}

	bsa::tes4::archive bsa_obj;
	fs::path bsa_path = data_dir / bsa_name;

	// skip BSA if it doesn't exist (can happen if it's in the ini but not in the data folder)
	if (!fs::exists(bsa_path)) {
		if(logging) {
			spdlog::warn("Skipping BSA {} because it doesn't exist", bsa_path.string());
		}
		return;
	}

	bsa::tes4::version bsa_version = bsa_obj.read(bsa_path);
	BSAFile bsa_struct = { bsa_path, bsa_version, bsa_obj };

	shared_ptr<BSAFile> bsa_shared_ptr = make_shared<BSAFile>(bsa_struct);

	// create itereator for bsa
	auto bsa_iter = bsa_obj.begin();
	// loop iterator
	for (auto bsa_iter = bsa_obj.begin(); bsa_iter != bsa_obj.end(); ++bsa_iter) {
		// get file entry from pointer
		const auto& file_entry = *bsa_iter;

		// get folder name within the BSA vfs
		const fs::path folder_name = file_entry.first.name();

		// .second stores the files in the folder
		const auto file_name = file_entry.second;

		// loop through files in folder
		for (const auto& entry : file_name) {
			// get name of file
			const string_view cur_entry = entry.first.name();
			const fs::path cur_path = folder_name / cur_entry;

			// chekc if we should ignore this file
			if (!file_allowed(cur_path)) {
				continue;
			}

			// add to filemap
			fileMap[cur_path] = bsa_shared_ptr;
		}
	}
}

vector<string> BethesdaDirectory::getBSAPriorityList() const
{
	// get bsa files not loaded from esp (also initializes output vector)
	vector<string> out_bsa_order = getBSAFilesFromINIs();

	// get esp priority list
	const vector<string> load_order = getPluginLoadOrder(true);

	// list BSA files in data directory
	const vector<string> all_bsa_files = getBSAFilesInDirectory();

	//loop through each esp in the priority list
	for (string plugin : load_order) {
		// add any BSAs to list
		vector<string> cur_found_bsas = findBSAFilesFromPluginName(all_bsa_files, plugin);
		concatenateVectorsWithoutDuplicates(out_bsa_order, cur_found_bsas);
	}

	if(logging) {
		// log output
		string bsa_list_str = boost::algorithm::join(out_bsa_order, ",");
		spdlog::debug("BSA Load Order: {}", bsa_list_str);

		for (string bsa : all_bsa_files) {
			if (find(out_bsa_order.begin(), out_bsa_order.end(), bsa) == out_bsa_order.end()) {
				spdlog::warn("BSA file {} not loaded by any plugin.", bsa);
			}
		}
	}

	return out_bsa_order;
}

vector<string> BethesdaDirectory::getPluginLoadOrder(bool trim_extension = false) const
{
	// initialize output vector. Stores
	vector<string> output_lo;

	// set path to loadorder.txt
	fs::path lo_file = getGameAppdataPath() / "loadorder.txt";

	// open file
	ifstream f = openFileHandle(lo_file, true);

	// loop through each line of loadorder.txt
	string line;
	while (getline(f, line)) {
		// Ignore lines that start with '#', which are comment lines
		if (line.empty() || line[0] == '#') {
			continue;
		}

		// Remove extension from line
		if (trim_extension) {
			line = line.substr(0, line.find_last_of('.'));
		}

		// Add to output list
		output_lo.push_back(line);
	}

	// close file handle
	f.close();

	if(logging) {
		string load_order_str = boost::algorithm::join(output_lo, ",");
		spdlog::debug("Plugin Load Order: {}", load_order_str);
	}

	// return output
	return output_lo;
}

vector<string> BethesdaDirectory::getBSAFilesFromINIs() const
{
	// output vector
	vector<string> bsa_files;

	// get ini properties
	boost::property_tree::ptree pt_ini = getINIProperties();

	// loop through each archive field in the INI files
	for (string field : ini_bsa_fields) {
		string cur_val;

		try {
			cur_val = pt_ini.get<string>("Archive." + field);
		}
		catch (const exception& e) {
			spdlog::info("Unable to find {} in [Archive] section in game ini: {}: Ignoring.", field, e.what());
			continue;
		}

		// split into components
		vector<string> ini_components;
		boost::split(ini_components, cur_val, boost::is_any_of(","));

		for (string bsa : ini_components) {
			// remove leading/trailing whitespace
			boost::trim(bsa);

			// add to output
			bsa_files.push_back(bsa);
		}

	}

	return bsa_files;
}

vector<string> BethesdaDirectory::getBSAFilesInDirectory() const
{
	vector<string> bsa_files;

	for (const auto& entry : fs::directory_iterator(this->data_dir)) {
		if (entry.is_regular_file()) {
			const string file_extension = entry.path().extension().string();
			// only interested in BSA files
			if (!boost::iequals(file_extension, ".bsa")) {
				continue;
			}

			// add to output
			bsa_files.push_back(entry.path().filename().string());
		}
	}

	return bsa_files;
}

vector<string> BethesdaDirectory::findBSAFilesFromPluginName(const vector<string>& bsa_file_list, const string& plugin_prefix) const
{
	vector<string> bsa_files_found;

	for (string bsa : bsa_file_list) {
		if (bsa.starts_with(plugin_prefix)) {
			if (bsa == plugin_prefix + ".bsa") {
				// load bsa with the plugin name before any others
				bsa_files_found.insert(bsa_files_found.begin(), bsa);
				continue;
			}

			// skip any BSAs that may start with the prefix but belong to a different plugin
			string after_prefix = bsa.substr(plugin_prefix.length());

			// todo: Is this actually how the game handles BSA files? Example: 3DNPC0.bsa, 3DNPC1.bsa, 3DNPC2.bsa are loaded, but 3DNPC - Textures.bsa is also loaded, whats the logic there?
			if (after_prefix.starts_with(" ") && !after_prefix.starts_with(" -")) {
				continue;
			}

			if (!after_prefix.starts_with(" ") && !isdigit(after_prefix[0])) {
				continue;
			}

			bsa_files_found.push_back(bsa);
		}
	}

	return bsa_files_found;
}

bool BethesdaDirectory::file_allowed(const fs::path file_path) const
{
	string file_extension = file_path.extension().string();
	boost::algorithm::to_lower(file_extension);
	if (std::find(extension_blocklist.begin(), extension_blocklist.end(), file_extension) != extension_blocklist.end()) {
		//elem exists in the vector
		return false;
	}

	return true;
}

boost::property_tree::ptree BethesdaDirectory::getINIProperties() const
{
	// get document path
	fs::path doc_path = getGameDocumentPath();

	// get ini file paths
	fs::path ini_path = doc_path / BethesdaGame::INILocations.at(game_type).ini;
	fs::path custom_ini_path = doc_path / BethesdaGame::INILocations.at(game_type).ini_custom;

	boost::property_tree::ptree pt_ini = readINIFile(ini_path, true);
	boost::property_tree::ptree pt_custom_ini = readINIFile(custom_ini_path, false);

	mergePropertyTrees(pt_ini, pt_custom_ini);

	return pt_ini;
}

//
// System Path Methods
//
fs::path BethesdaDirectory::getGameDocumentPath() const
{
	fs::path doc_path = getSystemPath(FOLDERID_Documents);
	doc_path /= BethesdaGame::DocumentLocations.at(this->game_type);
	return doc_path;
}

fs::path BethesdaDirectory::getGameAppdataPath() const
{
	fs::path appdata_path = getSystemPath(FOLDERID_LocalAppData);
	appdata_path /= BethesdaGame::AppDataLocations.at(this->game_type);
	return appdata_path;
}
